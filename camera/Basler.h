//Basler.h
//MVLZ ShenZhen MiaoWei 2016.11.30 �����ο����������ʣ��뼰ʱ��ϵ�� miaow@mvlz.com

#pragma once

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/gige/BaslerGigEInstantCamera.h>

// Namespace for using pylon objects.
using namespace Pylon;
// Namespace for using cout.
using namespace std;

//ע��:: ���������basler gige �ӿ����ʹ�ã�����������ӿ���������������û��в����߲���ʹ��
// �����ӿ����ʹ�ã���ν�ϱ����ӣ��ο�basler��װĿ¼������c++ ParametrizeCamera_NativeParameterAccess��
typedef Pylon::CBaslerGigEInstantCamera Camera_t;
using namespace Basler_GigECameraParams;
#include <pylon/PylonGUI.h>

typedef void (WINAPI *BaslerGrabbedCallback)(void* pOwner, int* width, int* height, unsigned char* pBuffer, bool isColor);

// COnImaggedTestDlg �Ի���
class CBaslerGigeNative: public CImageEventHandler,public Pylon::CConfigurationEventHandler
{
    // ����
public:
    CBaslerGigeNative();	// ��׼���캯��
    ~CBaslerGigeNative();

public:
    Camera_t camera;
    bool IsAcquisitionStartAvail;
    bool IsFrameStartStartAvail;
    bool IsCameraRemoved;
    void*			             m_pOwner;
    BaslerGrabbedCallback        m_DisplayImageCallBack;
    INT64        width;		// ͼ���
    INT64       height;		// ͼ���
    bool       isColor;		// �ж��Ƿ��ɫ���
    unsigned char * pImageBufferMono;	// �Զ���ڰ�ͼ��ָ��
    unsigned char * pImageBufferColor;	// �Զ����ɫͼ��ָ��

public:
    //����������������ʹ�Ƕ���������Ҳֻ�ܵ���һ��
    void PylonLibInit();
    void PylonLibTerminate();
    //���º����������ÿһ�����ʹ��
    BOOL OpenDevice(); //open first device by default;
    BOOL OpenDeviceBySn(string serialNumber); // open camera by specialized serial number
    BOOL OpenDeviceByUserID(string UserDefinedName);// open camera by user defined name
    BOOL OpenDeviceByIPAddress(string IPAddress, string subnetMask);//open camera by specilaized IP address
    void RemovedDeviceReconnect();// if device is removed, use this function to find it and connect it again
    void SetHeartbeatTimeout(int64_t valueMs); //ms, �����������pylon5.0.5�������˰汾����bug�ģ���ʱ��Ҫʹ��
    void CloseDevice();
    void GrabOne();
    void StartGrab();
    void StopGrab();
    void SetFreerunMode();
    void SetSoftwareTriggerMode();
    void SendSoftwareTriggerCommand();
    void SetExternalTriggerMode();
    void SetTriggerDelay(double nTimeUs);
    void SetLineDebouncerTimeAbs(double dTimeUs);
    void SetExposureTimeRaw(int64_t nExpTimeUs);
    void SetGainRaw(int64_t nGainRaw);
    void SetAcquisitionFrameRate(double fps);

    void SetOwner(void* pOwner, BaslerGrabbedCallback pDisplayImageCallBack);

    virtual void OnImageGrabbed( CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult);//included in CImageEventHandler
    //virtual void OnImagesSkipped( CInstantCamera& camera, size_t countOfSkippedImages); //included in CConfigurationEventHandler
    virtual void OnCameraDeviceRemoved(CInstantCamera& /*camera*/); //included in CImageEventHandler

};
