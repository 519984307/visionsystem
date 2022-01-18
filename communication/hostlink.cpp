﻿#include "hostlink.h"

HostLink::HostLink(QObject *parent) : QObject(parent)
{
    serialPort=new QSerialPort(this);
    connect(serialPort,&QSerialPort::readyRead,this,&HostLink::slot_startTimer);
    connect(serialPort, &QSerialPort::errorOccurred, this, &HostLink::slot_readError);

    _p_read_write_timer=new QTimer(this);
    connect(_p_read_write_timer, &QTimer::timeout, this, &HostLink::slot_read_write);

    _p_write_timer=new QTimer(this);
    connect(_p_write_timer, &QTimer::timeout, this, &HostLink::slot_timeto_write);

    _p_timer_rev=new QTimer(this);
    connect(_p_timer_rev, &QTimer::timeout, this, &HostLink::slot_readyRead);
}
HostLink::~HostLink()
{
    disconnectHostLink();
    if(_p_timer_rev!=nullptr)
    {
        _p_timer_rev->stop();
        delete _p_timer_rev;
        _p_timer_rev=nullptr;
    }
    if(serialPort!=nullptr)
    {
        delete serialPort;
        serialPort=nullptr;
    }
    if(_p_read_write_timer!=nullptr)
    {
        _p_read_write_timer->stop();
        delete _p_read_write_timer;
        _p_read_write_timer=nullptr;
    }
    if(_p_write_timer!=nullptr)
    {
        delete _p_write_timer;
        _p_write_timer=nullptr;
    }
}
void HostLink::initPar(QString comname,QString portName, QString parity, int baud, int databits, int stopbit,int readspace,bool readsignal,QMap<QString,int> addrInfo)
{
    _isAnswer=false;
    _name=comname;
    _portName=portName;
    _parity=parity;
    _baudRate=baud;
    _databit=databits;
    _stopbit=stopbit;
    _read_startaddr=addrInfo[QString::fromLocal8Bit("读取起始地址")];
    _read_size=addrInfo[QString::fromLocal8Bit("读取长度")];
    _write_startaddr=addrInfo[QString::fromLocal8Bit("写入起始地址")];
    _write_size=addrInfo[QString::fromLocal8Bit("写入长度")];
    _read_space=readspace;
    _is_read_TriggerSignal=readsignal;
    _is_write_data_change=false;
    zeroQList(&_read_data,_read_size);
    zeroQList(&_write_data,_write_size);

    _p_read_write_timer->start(_read_space);

}

//初始化fx3U的通讯
int8_t HostLink::initHostLink()
{
    //初始化串口
    if(serialPort->isOpen())//如果串口已经打开 先关闭
    {
        serialPort->close();
    }
    serialPort->setPortName(_portName);
    if(!serialPort->open(QIODevice::ReadWrite)) return 1;
    serialPort->setBaudRate(_baudRate,QSerialPort::AllDirections);//设置波特率为115200
    switch(_databit)
    {
    case 6 :
        serialPort->setDataBits(QSerialPort::Data6);//设置数据位8
        break;
    case 7 :
        serialPort->setDataBits(QSerialPort::Data7);//设置数据位8
        break;
    case 8 :
        serialPort->setDataBits(QSerialPort::Data8);//设置数据位8
        break;
    }
    if(_parity=="No")
    {
        serialPort->setParity(QSerialPort::NoParity); //无校验
    }
    else if(_parity=="Odd")
    {
        serialPort->setParity(QSerialPort::OddParity); //奇校验
    }
    else if(_parity=="Even")
    {
        serialPort->setParity(QSerialPort::EvenParity); //偶校验
    }
    else if(_parity=="Space")
    {
        serialPort->setParity(QSerialPort::SpaceParity); //校验位始终为0
    }
    else if(_parity=="Mark")
    {
        serialPort->setParity(QSerialPort::MarkParity); //校验位始终为1
    }
    switch(_stopbit)
    {
    case 1 :
        serialPort->setStopBits(QSerialPort::OneStop);//停止位设置为1
        break;
    case 2 :
        serialPort->setStopBits(QSerialPort::TwoStop);//停止位设置为2
        break;
    }
    serialPort->setFlowControl(QSerialPort::NoFlowControl);//设置为无流控制
    return 0;
}

//断开连接
bool HostLink::disconnectHostLink()
{
    _isConnected=false;
    if(serialPort==nullptr)return true;
    if(serialPort->isOpen())
    {
        serialPort->close();
    }
    return 0;
}
void HostLink::slot_startTimer()
{
    _p_timer_rev->start(20);
}
//接收数据并转化为int数组
void HostLink::slot_readyRead()
{
    _p_timer_rev->stop();
    QByteArray readByte = serialPort->readAll();
    if(!readByte.isEmpty())
    {
        if(readByte.size()<5)return;
        if(readByte.at(0)==0x06)
        {
            //g_is_allow_write=true;
        }
        _isAnswer=true;
        emit signal_com_state(_name,_isAnswer);
        if(_isWrite)
        {
            _isWrite=false;
            return;
        }
        if(readByte.at(0)==0x02 && readByte.at(readByte.size()-3)==0x03)
        {
            //g_is_allow_read=true;
            QByteArray buf=readByte.mid(5,readByte.size()-8);
            QList<int> data=transToIntList(buf);
            QList<int> compData=compareReaddata(data,_read_data);
            _read_data=data;
            emit signal_readFinish(compData,_name);
        }
    }
}
QList<int> HostLink::compareReaddata(QList<int> data1,QList<int> data2)
{
    if(data1.size()!=data2.size())return QList<int>();
    QList<int> data;
    for (int i=0;i<data1.size();i++)
    {
        if(data1.at(i)==data2.at(i))
        {
            data<<0;
        }
        else
        {
            data<<data1.at(i);
        }
    }
    return data;
}
void HostLink::slot_readError()
{
    _isConnected=false;
    //emit signal_com_state(_name,false);
}


void HostLink::slot_write(QString comname,int startaddr, QList<int> writedata)
{
    if(_name!=comname)return;
    for (int i=startaddr;i<startaddr+writedata.size();i++)
    {
        _write_data[i]=writedata[i-startaddr];
    }
    _is_write_data_change=true;
}

void HostLink::slot_read(QString comname)
{
    if(_name!=comname)return;
    readHostlinkCIOWord(_read_startaddr,_read_size);
}
//串口发送字符
void HostLink::sendStr(QString sendText)
{
    _isAnswer=false;
    emit signal_com_state(_name,_isAnswer);
    QByteArray byte=sendText.toLocal8Bit();
    int a=serialPort->write(byte); //qt5去除了.toAscii()
}
void HostLink::zeroQList(QList<int> *list, int size)
{
    (*list).clear();
    for (int i=0;i<size;i++)
    {
        (*list)<<0;
    }
}
void HostLink::slot_read_write()
{
    if(!_isAnswer)
    {
        disconnectHostLink();
        initHostLink();
        readHostlinkCIOWord(100,1);
        return;
    }
    if(_is_read_TriggerSignal)
    {
        readHostlinkCIOWord(_read_startaddr,_read_size);
    }
    _p_write_timer->start(_read_space/2);



}

void HostLink::slot_timeto_write()
{
    //if(_is_write_data_change)
    {
        _isWrite=true;
        writeHostlinkCIOWord(_write_startaddr,_write_data);
        _is_write_data_change=false;
        zeroQList(&_write_data,_write_size);
    }
    _p_write_timer->stop();
}
void HostLink::slot_com_start()
{
    _p_read_write_timer->start(_read_space);
}

void HostLink::slot_com_stop()
{
    _p_read_write_timer->stop();

}

//写cio位
void HostLink::writeHostlinkCIOBit(int startAddr,int bit,QList<int> dataWrite)
{
    int count=dataWrite.size();
    QString str="@00FA0000000000102"+QString("30")+
            QString("%1").arg(startAddr,4,16,QLatin1Char('0'))+
            QString("%1").arg(bit,2,16,QLatin1Char('0'))+
            QString("%1").arg(count,4,16,QLatin1Char('0'));
    for (int a=0;a<dataWrite.size();a++)
    {
        str+=QString("%1").arg(dataWrite[a],2,16,QLatin1Char('0'));
    }
    QString strsend=str + computeFCS(str)+"*\r";
    sendStr(strsend);
}
//读cio位
void HostLink::readHostlinkCIOBit(int startAddr,int bit,int count)
{
    QString str="@00FA0000000000101"+QString("30")+
            QString("%1").arg(startAddr,4,16,QLatin1Char('0'))+
            QString("%1").arg(bit,2,16,QLatin1Char('0'))+
            QString("%1").arg(count,4,16,QLatin1Char('0'));

    QString strsend=str + computeFCS(str)+"*\r";
    sendStr(strsend);
}
//写CIO字
void HostLink::writeHostlinkCIOWord(int startAddr,QList<int> dataWrite)
{
    QString str="@00FA0000000000102"+QString("B0")+
            QString("%1").arg(startAddr,4,16,QLatin1Char('0'))+
            "00"+
            QString("%1").arg(dataWrite.size(),4,16,QLatin1Char('0'));
    for (int a=0;a<dataWrite.size();a++)
    {
        str+=QString("%1").arg(dataWrite[a],4,16,QLatin1Char('0'));
    }
    QString strsend=str + computeFCS(str)+"*\r";
    sendStr(strsend);
}

//读CIO字
void HostLink::readHostlinkCIOWord(int startAddr,int count)
{
    QString str="@00FA0000000000101"+QString("B0")+
            QString("%1").arg(startAddr,4,16,QLatin1Char('0'))+
            "00"+
            QString("%1").arg(count,4,16,QLatin1Char('0'));
    QString strsend=str + computeFCS(str)+"*\r";
    sendStr(strsend);
}
//
void HostLink::writeHostlinkWBit(int startAddr,int bit,QList<int> dataWrite)
{
    int count=dataWrite.size();
    QString str="@00FA0000000000102"+QString("31")+
            QString("%1").arg(startAddr,4,16,QLatin1Char('0'))+
            QString("%1").arg(bit,2,16,QLatin1Char('0'))+
            QString("%1").arg(count,4,16,QLatin1Char('0'));
    for (int a=0;a<dataWrite.size();a++)
    {
        str+=QString("%1").arg(dataWrite[a],2,16,QLatin1Char('0'));
    }
    QString strsend=str + computeFCS(str)+"*\r";
    sendStr(strsend);
}
void HostLink::readHostlinkWBit(int startAddr,int bit,int count)
{
    QString str="@00FA0000000000101"+QString("31")+
            QString("%1").arg(startAddr,4,16,QLatin1Char('0'))+
            QString("%1").arg(bit,2,16,QLatin1Char('0'))+
            QString("%1").arg(count,4,16,QLatin1Char('0'));
    QString strsend=str + computeFCS(str)+"*\r";
    sendStr(strsend);
}

void HostLink::writeHostlinkWWord(int startAddr,QList<int> dataWrite)
{
    int count=dataWrite.size();
    QString str="@00FA0000000000102"+QString("B1")+
            QString("%1").arg(startAddr,4,16,QLatin1Char('0'))+
            "00"+
            QString("%1").arg(count,4,16,QLatin1Char('0'));
    for (int a=0;a<dataWrite.size();a++)
    {
        str+=QString("%1").arg(dataWrite[a],4,16,QLatin1Char('0'));
    }
    QString strsend=str + computeFCS(str)+"*\r";
    sendStr(strsend);
}

void HostLink::readHostlinkWWord(int startAddr,int count)
{
    QString str="@00FA0000000000101"+QString("B1")+
            QString("%1").arg(startAddr,4,16,QLatin1Char('0'))+
            "00"+
            QString("%1").arg(count,4,16,QLatin1Char('0'));
    QString strsend=str + computeFCS(str)+"*\r";
    sendStr(strsend);
}

QString HostLink::computeFCS(QString linkstring)
{
    char inFcs =char(linkstring.toStdString().at(0));
    int fcsResult = int(inFcs);
    for (int i = 1; i < linkstring.length(); i++)
    {
        inFcs = char((linkstring.toStdString().at(i)));
        fcsResult = fcsResult^int(inFcs);
    }
    QString aa=QString("%1").arg(fcsResult,2,16,QLatin1Char('0'));
    return aa.right(2);
}

//将bytearray转化为int数组
QList<int> HostLink::transToIntList(QByteArray buf)
{
    QList<int> intdata=QList<int>();
    for (int i=0;i<buf.size()/2;i++)
    {
        QByteArray array;
        array.resize(2);
        array[0]=buf[i*2];
        array[1]=buf[i*2+1];
        bool ok;
        int data=array.toInt(&ok,16);
        intdata<<data;
    }
    return intdata;
}
