﻿
#include"deeplearning/Classified/classified.h"
#include "pro_connectseatglue.h"
#include <QApplication>
#include"general/configfileoperate.h"
#include"general/generalfunc.h"
#include"communication/communication.h"
#include"camera/camerapar.h"
#include<QThread>
#include<QTime>
#include"parameterset.h"
#include"general/xmloperate.h"
#include"deeplearning/Classified/classfiedopenvino.h"
#include<QMessageBox>

Pro_ConnectSeatGlue::Pro_ConnectSeatGlue(QObject *parent) : QObject(parent)
{
    m_ExePath=qApp->applicationDirPath();
    check_err=false;
    HalconCpp::GenEmptyObj(&m_OriginImage);
    HalconCpp::GenEmptyObj(&m_SceneImage);
    HalconCpp::GenEmptyObj(&m_SceneImageShow);
    SetWindowAttr("background_color","black");
}

Pro_ConnectSeatGlue::~Pro_ConnectSeatGlue()
{
    slot_program_exit();
}
//初始化参数
void Pro_ConnectSeatGlue::initGlobalPar(QString pro,QString station,QString cam,QString step,QString type)
{
    m_ProName=pro;
    m_ProPath=m_ExePath+"\\Pro\\"+m_ProName;
    m_StationName=station;
    m_station_path=m_ProPath+"\\station\\"+m_StationName; //工位路径
    m_CamName=cam;
    m_CamPath=m_station_path+"\\"+m_CamName;
    m_StepName=step;
    m_StepPath=m_CamPath+"\\"+m_StepName;
    m_TypeName=type;
    m_TypePath=m_StepPath+"\\"+m_TypeName;
    if(!GeneralFunc::isDirExist(m_TypePath,false))return;
    m_ReportPath=m_ExePath+"\\Report\\"+m_ProName+"\\"+m_StationName+"\\"+m_CamName+"\\"+m_StepName+"\\"+m_TypeName;
    readROI();
    readPar();
    //readTypeInfo();
    openHalconWindow();
}
void Pro_ConnectSeatGlue::openHalconWindow()
{
    QStringList sizelist=m_cam_info.resolution.split(':');
    //打开一个隐藏得窗口用于绘图
    HTuple len;
    TupleLength(hv_WindowHandle_UI,&len);
    if(len>0)
    {
        CloseWindow(hv_WindowHandle_UI);
    }

        OpenWindow(0,0,sizelist[0].toInt(),sizelist[1].toInt(),0,"invisible","",&hv_WindowHandle_UI);
}
//根据启用的品类，读取选区信息
void Pro_ConnectSeatGlue::readROI()
{
    QString roiPath=m_TypePath+"\\roi";
    map_hobj.clear();
    QStringList  roilist=GeneralFunc::getFileListName(roiPath,QStringList()<<"*.*");;
    for (int i=0;i<roilist.size();i++)
    {

        QStringList roi_type=roilist[i].split('#');
        if(roi_type.size()<2)continue;
        if(roi_type[0].mid(0,8)=="modelroi")
        {
            XmlOperate m_xml;
            m_xml.openXml(m_TypePath+"\\ROIConfig.xml");
            map_model_par[roi_type[0]].angle_Start=m_xml.readNode(QStringList()<<roi_type[0]<<"Angle_Start").toDouble();
            map_model_par[roi_type[0]].min_Score=m_xml.readNode(QStringList()<<roi_type[0]<<"Min_Score").toDouble();
            map_model_par[roi_type[0]].numToFind=m_xml.readNode(QStringList()<<roi_type[0]<<"NumToFind").toInt();
            map_model_par[roi_type[0]].max_Overlap=m_xml.readNode(QStringList()<<roi_type[0]<<"Max_Overlap").toDouble();
            map_model_par[roi_type[0]].sub_Pixel=m_xml.readNode(QStringList()<<roi_type[0]<<"Sub_Pixel").toLocal8Bit().data();
            map_model_par[roi_type[0]].num_Levels=QtHalcon::qstringToHtuple(m_xml.readNode(QStringList()<<roi_type[0]<<"Num_Levels"),',',"int");
            map_model_par[roi_type[0]].greediness=m_xml.readNode(QStringList()<<roi_type[0]<<"Greediness").toDouble();
            if(GeneralFunc::isFileExist(QString(m_TypePath+"\\"+roi_type[0]+".shm"),0))
            {
                HalconCpp::ReadShapeModel(QString(m_TypePath+"\\"+roi_type[0]+".shm").toLocal8Bit().data(),&map_model_par[roi_type[0]].modelID);
            }
            map_model_par[roi_type[0]].row_M=m_xml.readNode(QStringList()<<roi_type[0]<<"Row_M").toDouble();//模板位置
            map_model_par[roi_type[0]].col_M=m_xml.readNode(QStringList()<<roi_type[0]<<"Col_M").toDouble();
            map_model_par[roi_type[0]].ang_M=m_xml.readNode(QStringList()<<roi_type[0]<<"Ang_M").toDouble();
            map_model_par[roi_type[0]].movementOfObject_Model_M=QtHalcon::qstringToHtuple(m_xml.readNode(QStringList()<<roi_type[0]<<"MovementOfObject_Model_M"),',',"double");//模板变换矩阵
            m_xml.closeXml();
            continue;
        }

        switch (EnumChange::string2enum_roitype(roi_type[1]))
        {
        case EnumChange::Rectangle1:
        case EnumChange::Rectangle2:
        case EnumChange::Circle:
        case EnumChange::Polygon:
        case EnumChange::Cirque:
            if(GeneralFunc::isFileExist(QString(roiPath+"\\"+roilist[i]+".hobj"),0))
            {
                ReadObject(&map_hobj[roilist[i]],QString(roiPath+"\\"+roilist[i]+".hobj").toLocal8Bit().data());
            }
            break;
        case EnumChange::Nurb:
        case EnumChange::Xld:
            if(GeneralFunc::isFileExist(QString(roiPath+"\\"+roilist[i]),0))
            {
                ReadObject(&map_hobj[roilist[i]],QString(roiPath+"\\"+roilist[i]).toLocal8Bit().data());
            }
            break;
        }
    }

}

//读取品种文件
void Pro_ConnectSeatGlue::readTypeInfo()
{
    //slot_program_exit();
    map_Thread.clear();
    if(m_StringParameter[QString::fromLocal8Bit("处理器")]=="GPU")
    {
        map_Classified.clear();
        m_weightfile= m_TypePath+"\\best.torchscript_cuda.pt";//权重文件
        m_namesfile= m_TypePath+"\\voc.names";//权重文件
        std::string weights=m_weightfile.toStdString();
        std::string name_file =m_namesfile.toStdString();
        if(!GeneralFunc::isFileExist(m_weightfile,false)||!GeneralFunc::isFileExist(m_namesfile,false))
        {
            emit  signal_information_text(m_CamName,1,QString::fromLocal8Bit("不存在权重文件"));
        }
        else
        {
            for (int i=0;i<map_hobj.size();i++)
            {
                map_Classified[i]=new Classified(nullptr,i,weights,name_file);
                map_Thread[i]=new QThread(this);
                map_Classified[i]->moveToThread(map_Thread[i]);
                connect(map_Thread[i],&QThread::finished,map_Thread[i],&QObject::deleteLater);
                connect(map_Thread[i],&QThread::finished,map_Classified[i],&QObject::deleteLater);
                connect(this,&Pro_ConnectSeatGlue::signal_check_image,map_Classified[i],&Classified::slot_run);
                connect(this,&Pro_ConnectSeatGlue::signal_setCheckPar,map_Classified[i],&Classified::slot_setCheckPar);
                connect(map_Classified[i],&Classified::signal_result,this,&Pro_ConnectSeatGlue::slot_check_result);
                map_Thread[i]->start();
            }
        }
    }
    else if(m_StringParameter[QString::fromLocal8Bit("处理器")]=="CPU")
    {
        map_openvino.clear();
        m_weightfile= m_TypePath+"\\best.onnx";//权重文件
        m_namesfile= m_TypePath+"\\voc.names";//权重文件
        std::string weights=m_weightfile.toStdString();
        std::string name_file =m_namesfile.toStdString();
        if(!GeneralFunc::isFileExist(m_weightfile,false)||!GeneralFunc::isFileExist(m_namesfile,false))
        {
            emit  signal_information_text(m_CamName,1,QString::fromLocal8Bit("不存在权重文件"));
        }
        else
        {
            for (int i=0;i<map_hobj.size();i++)
            {
                map_openvino[i]=new classfiedopenvino(nullptr,i,"CPU",weights,name_file);
                map_Thread[i]=new QThread(this);
                map_openvino[i]->moveToThread(map_Thread[i]);
                connect(map_Thread[i],&QThread::finished,map_Thread[i],&QObject::deleteLater);
                connect(map_Thread[i],&QThread::finished,map_openvino[i],&QObject::deleteLater);
                connect(this,&Pro_ConnectSeatGlue::signal_check_image,map_openvino[i],&classfiedopenvino::slot_run);
                connect(this,&Pro_ConnectSeatGlue::signal_setCheckPar,map_openvino[i],&classfiedopenvino::slot_setCheckPar);
                connect(map_openvino[i],&classfiedopenvino::signal_result,this,&Pro_ConnectSeatGlue::slot_check_result);
                map_Thread[i]->start();
            }
        }
    }
}
//根据启用的品类读取参数信息
void Pro_ConnectSeatGlue::readPar()
{
    XmlOperate m_xml_par;
    m_xml_par.openXml(m_TypePath+"\\ParConfig.xml");
    QStringList parTypeList=QStringList()<<"IntPar"<<"FloatPar"<<"StringPar";
    QStringList parlist=m_xml_par.getChild(QStringList()<<parTypeList[0]);
    for (int i=0;i<parlist.size();i++)
    {
        m_IntParameter[parlist[i]]=m_xml_par.readNode(QStringList()<<parTypeList[0]<<parlist[i]).toInt();
    }
    parlist=m_xml_par.getChild(QStringList()<<parTypeList[1]);
    for (int i=0;i<parlist.size();i++)
    {
        m_FloatParameter[parlist[i]]=m_xml_par.readNode(QStringList()<<parTypeList[1]<<parlist[i]).toFloat();
    }
    parlist=m_xml_par.getChild(QStringList()<<parTypeList[2]);
    for (int i=0;i<parlist.size();i++)
    {
        m_StringParameter[parlist[i]]=m_xml_par.readNode(QStringList()<<parTypeList[2]<<parlist[i]);
    }
    m_xml_par.closeXml();

    //读取相机配置
    QString xmlPath=m_ProPath+"\\camera\\CamConfig.xml";
    XmlOperate m_xml;
    m_xml.openXml(xmlPath);
    m_cam_info.resolution=m_xml.readNode(QStringList()<<m_CamName<<"Resolution");
    QStringList sizelist=m_cam_info.resolution.split(':');
    if(sizelist.size()!=2)
    {
        m_cam_info.resolution="2448:2048";
        sizelist=m_cam_info.resolution.split(':');
    }
    int image_width=sizelist[0].toInt();
    int image_height=sizelist[1].toInt();
    if(m_IntParameter[QString::fromLocal8Bit("校正图像")]==1)
    {
        m_cam_info.map_step_cal[m_StepName].isCal=m_xml.readNode(QStringList()<<m_CamName<<m_StepName<<"IsCal");
        if(m_cam_info.map_step_cal[m_StepName].isCal=="1")
        {
            m_cam_info.map_step_cal[m_StepName].ratio=m_xml.readNode(QStringList()<<m_CamName<<m_StepName<<"Ratio");
            m_cam_info.map_step_cal[m_StepName].camPar=m_xml.readNode(QStringList()<<m_CamName<<m_StepName<<"CamPar");
            m_cam_info.map_step_cal[m_StepName].camPose=m_xml.readNode(QStringList()<<m_CamName<<m_StepName<<"CamPose");
            m_cam_info.map_step_cal[m_StepName].X_Offset=m_xml.readNode(QStringList()<<m_CamName<<m_StepName<<"X_Offset");
            m_cam_info.map_step_cal[m_StepName].Y_Offset=m_xml.readNode(QStringList()<<m_CamName<<m_StepName<<"Y_Offset");
            m_cam_info.map_step_cal[m_StepName].worldWidth=m_xml.readNode(QStringList()<<m_CamName<<m_StepName<<"WorldWidth");
            m_cam_info.map_step_cal[m_StepName].worldHeight=m_xml.readNode(QStringList()<<m_CamName<<m_StepName<<"WorldHeight");
            HTuple hv_cam_par=QtHalcon::qstringToHtuple(m_cam_info.map_step_cal[m_StepName].camPar,',',"double");
            HTuple  hv_cam_pose=QtHalcon::qstringToHtuple(m_cam_info.map_step_cal[m_StepName].camPose,',',"double");
            QtHalcon::calibrateImage(&m_MapFix, image_width, image_height,
                                     m_cam_info.map_step_cal[m_StepName].worldWidth.toDouble(),m_cam_info.map_step_cal[m_StepName].worldHeight.toDouble(),
                                     hv_cam_par,hv_cam_pose,
                                     m_cam_info.map_step_cal[m_StepName].ratio.toDouble(),
                                     m_cam_info.map_step_cal[m_StepName].X_Offset.toDouble(), m_cam_info.map_step_cal[m_StepName].Y_Offset.toDouble());
        }
    }
    m_xml.closeXml();
    //删除超出保存日期的记录
    GeneralFunc::deleteFolderOutOfRange(m_ReportPath,m_IntParameter[QString::fromLocal8Bit("保存天数")]);
    readTypeInfo();
}

//计算点击处的像素信息
void Pro_ConnectSeatGlue::slot_press_point(QString cam,QPoint point, QSize size)
{
    if(cam!=m_CamName)
        return;
    try
    {
        if(QtHalcon::testHObjectEmpty(m_SceneImage))return;
        //计算目标处的灰度值，并显示到状态栏标签
        HTuple grayValue,pos,grayValue_H;
        QtHalcon::getMousePosGray(m_SceneImage,point,size,&pos,&grayValue);
        if(grayValue.TupleLength()==1)
        {
            int r_value;
            r_value=grayValue.I();
            m_runInfo=QString("(%1,%2) RGB:%3,%4,%5").arg(QString::number(pos[0].I()),QString::number(pos[1].I()),
                    QString::number(r_value),QString::number(r_value),
                    QString::number(r_value));
            emit  signal_information_text(m_CamName,2,m_runInfo);
        }
        else if(grayValue.TupleLength()==3)
        {
            int r_value,g_value,b_value;
            r_value=grayValue[0].I();
            g_value=grayValue[1].I();
            b_value=grayValue[2].I();
            m_runInfo=QString("(%1,%2) RGB:%3,%4,%5").arg(QString::number(pos[0].I()),QString::number(pos[1].I()),
                    QString::number(r_value),QString::number(g_value),QString::number(b_value));
            emit  signal_information_text(m_CamName,2,m_runInfo);
        }
    }
    catch (cv::Exception & e)
    {

    }
}
//接收一帧
void Pro_ConnectSeatGlue::slot_one_frame(QImage src,QString camname,bool ischeck,QString step)
{
    if(camname!=m_CamName||step!=m_StepName)return;
    if(src.isNull()) return;
    QtHalcon::qImage2HImage(src,m_OriginImage);
    CopyImage(m_OriginImage,&m_SceneImage);
    CopyImage(m_OriginImage,&m_SceneImageShow);
    if(ischeck)
    {
        slot_check(m_CamName,step);
    }
    else
    {
        emit signal_information_image(m_CamName,0,src);
    }
}

//检测
void Pro_ConnectSeatGlue::slot_check(QString camName,QString step)
{
    if(camName!=m_CamName||step!=m_StepName)return;
    if(QtHalcon::testHObjectEmpty(m_SceneImage))return;

    time_start = double(clock());//开始计时

    v_obj_result.clear();
    v_obj_bool.clear();
    m_send_data.clear();
    v_result_image.clear();
    QMap<QString,HObject>::iterator iter=map_hobj.begin();
    int count=-1;
    while(iter!=map_hobj.end())
    {
        count++;
        QString key=iter.key();
        HObject value=iter.value();
        try
        {
            QStringList name_type_list=key.split('#');
            if(name_type_list.size()<1)continue;
            HalconCpp::HObject image_part;
            HObject image_reduce,rect;
            HalconCpp::ReduceDomain(m_SceneImage,value,&image_reduce);
            HalconCpp::CropDomain(image_reduce,&image_part);
            v_obj_result<<value;
            v_result_image<<image_part;
            v_obj_bool.push_back(0);
            //checkFunc
            cv::Mat check_img=QtHalcon::HObjectToMat(image_part);
            emit signal_setCheckPar(count,check_img,m_IntParameter[QString::fromLocal8Bit("图像大小")]);

            emit signal_check_image();

        }
        catch (cv::Exception &e)
        {
            emit  signal_information_text(m_CamName,1,"cv");
            continue;
        }
        catch (HalconCpp::HException &e)
        {
            emit  signal_information_text(m_CamName,1,"halcon");
            continue;
        }
        iter++;
    }
    if(v_result_image.size()<1)
    {
        m_send_data<<2<<0;
        check_err=true;
        slot_check_result(0,QString());
    }
}
void Pro_ConnectSeatGlue::slot_check_result(int num,QString name)
{
    if(!check_err)
    {
        v_obj_bool[num]=name.toInt();
        check_count++;
        if(check_count!=v_obj_result.size())return;
        check_count=0;
        if(v_obj_bool.contains(3)||v_obj_bool.contains(4))
        {
            m_send_data<<name.toInt();
        }
        else
        {
            m_send_data<<name.toInt();
        }
    }

    send_result();
    time_end = double(clock());
    m_runInfo=tr(QString::fromLocal8Bit("%1检测时间:%2ms").arg(m_CamName,QString::number(time_end-time_start)).toUtf8());
    emit  signal_information_text(m_CamName,1,m_runInfo);

    QStringList result;
    for (int i=0;i<v_obj_bool.size();i++)
    {
        result<<QString::number(v_obj_bool[i]);
    }
    m_runInfo=tr(QString::fromLocal8Bit("%1检测结果:%2").arg(m_CamName,result.join("|")).toUtf8());
    emit  signal_information_text(m_CamName,1,m_runInfo);

    time_start = double(clock());
    writeCheckInfoToImage();
    time_end = double(clock());
    m_runInfo=tr(QString::fromLocal8Bit("%1绘制时间:%2ms").arg(m_CamName,QString::number(time_end-time_start)).toUtf8());
    emit  signal_information_text(m_CamName,1,m_runInfo);

    time_start = double(clock());
    writeImageAndReport();
    time_end = double(clock());
    m_runInfo=tr(QString::fromLocal8Bit("%1保存时间:%2ms").arg(m_CamName,QString::number(time_end-time_start)).toUtf8());
    emit  signal_information_text(m_CamName,1,m_runInfo);

    check_err=false;
}

//读取文件
void Pro_ConnectSeatGlue::slot_read_image(QString cam,QString step,QString imgPath)
{
    if(cam==m_CamName&&step==m_StepName)
    {
        HalconCpp::ReadImage(&m_OriginImage,imgPath.toLocal8Bit().data());
        if(QtHalcon::testHObjectEmpty(m_OriginImage))return;
        QImage image;
        QtHalcon::hImage2QImage(m_OriginImage,image);
        CopyImage(m_OriginImage,&m_SceneImage);
        CopyImage(m_OriginImage,&m_SceneImageShow);
        emit signal_information_image(m_CamName,0,image);
    }
}

//写入记录
void Pro_ConnectSeatGlue::writeImageAndReport()//写入图片和记录
{
    QDateTime date=QDateTime::currentDateTime();
    QString dateDir;
    dateDir=m_ReportPath+"\\"+date.toString("yyMMdd");
    if(m_IntParameter[QString::fromLocal8Bit("保存检测图像")]==1)
    {
        GeneralFunc::isDirExist(dateDir,true);
        QString timePath=dateDir+"\\"+date.toString("hhmmss")+".jpg";
        QtHalcon::saveHObject(m_SceneImageShow,"jpg",timePath.toLocal8Bit().data());
        for (int i=0;i<v_obj_bool.size();i++)
        {
            timePath=dateDir+"\\"+QString::number(v_obj_bool[i])+"\\"+date.toString("hhmmss-")+QString::number(i)+".jpg";
            GeneralFunc::isDirExist(dateDir+"\\"+QString::number(v_obj_bool[i]),true);
            QtHalcon::saveHObject(v_result_image[i],"jpg",timePath.toLocal8Bit().data());
        }
    }
    if(m_IntParameter[QString::fromLocal8Bit("保存原始图像")]==1)
    {
        QString timePath=dateDir+"\\origin"+date.toString("hhmmss")+".jpg";
        GeneralFunc::isDirExist(dateDir,true);
        QtHalcon::saveHObject(m_OriginImage,"jpg",timePath.toLocal8Bit().data());
    }
}


//写或绘制检测信息到虚拟窗口
void Pro_ConnectSeatGlue::writeCheckInfoToImage()//写检测结果到图片
{
    try
    {
        if(m_IntParameter[QString::fromLocal8Bit("绘制边缘宽度")]<1)
        {
            emit  signal_information_text(m_CamName,1,QString::fromLocal8Bit("绘制图像宽度未设置"));
            QImage image;
            QtHalcon::hImage2QImage(m_OriginImage,image);
            emit signal_information_image(m_CamName,v_obj_bool[0],image);
            return;
        }
        QtHalcon::displayHObject(m_SceneImage,hv_WindowHandle_UI);
        SetDraw(hv_WindowHandle_UI,"margin");
        SetLineWidth(hv_WindowHandle_UI,m_IntParameter[QString::fromLocal8Bit("绘制边缘宽度")]);//5
        for(int i=0;i<v_obj_result.size();i++)
        {
            QString colorstr;
            v_obj_bool[i]==1?colorstr="green":colorstr="red";
            SetColor(hv_WindowHandle_UI,colorstr.toLocal8Bit().data());
            DispObj(v_obj_result[i],hv_WindowHandle_UI);
        }
        DumpWindowImage(&m_SceneImageShow,hv_WindowHandle_UI);
        QImage image;
        QtHalcon::hImage2QImage(m_SceneImageShow,image);
        ClearWindow(hv_WindowHandle_UI);
        if(v_obj_bool.contains(3)||v_obj_bool.contains(4))
        {
            emit signal_information_image(m_CamName,2,image);
        }
        else
        {
            emit signal_information_image(m_CamName,1,image);
        }
    }
    catch (HalconCpp::HException & ecp)
    {
    }
}

//发送结果
void Pro_ConnectSeatGlue::send_result()
{
    emit signal_result(m_send_data,m_CamName,m_StepName);
}
//选区变化
void Pro_ConnectSeatGlue::slot_roi_change(QString cam,QString step)
{
    if(cam==m_CamName&&m_StepName==step)
    {
        readROI();
    }
}
//品种变化
void Pro_ConnectSeatGlue::slot_type_change(QString type)
{
    m_TypeName=type;
    m_TypePath=m_StepPath+"\\"+m_TypeName;
    if(!GeneralFunc::isDirExist(m_TypePath,false))return;
    m_ReportPath=m_ExePath+"\\Report\\"+m_ProName+"\\"+m_StationName+"\\"+m_CamName+"\\"+m_StepName+"\\"+m_TypeName;
    readROI();
    readPar();
}
//参数变化
void Pro_ConnectSeatGlue::slot_par_change(QString cam,QString step)
{
    if(cam==m_CamName&&m_StepName==step)
    {
        readPar();
    }
}
//程序结束
void Pro_ConnectSeatGlue::slot_program_exit()
{
    if(map_Thread.size()<1)return;
    QMap<int,QThread*>::iterator iter=map_Thread.begin();
    while (iter!=map_Thread.end())
    {
        if(map_Thread[iter.key()]!=nullptr)
        {
            map_Thread[iter.key()]->exit(0);
            map_Thread[iter.key()]->wait();
            delete map_Thread[iter.key()];
            map_Thread[iter.key()]=nullptr;

        }
        iter++;
    }

}




