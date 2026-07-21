using System;
using System.Runtime.InteropServices;

namespace NMRDLL
{
    public static class NMR
    {
        const string dllFullName = "mridll.dll";

        [DllImport(dllFullName)]
        public static extern int add(int a, int b);
        [DllImport(dllFullName)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool GetArmExeUpdateStatus();
        [DllImport(dllFullName)]
        public static extern double FilterParGenerate(double fin, bool saveFilterfile);
        [DllImport(dllFullName)]
        public static extern IntPtr GetDLLPath();
        [DllImport(dllFullName)]
        public static extern IntPtr GetCPLDVer();
        [DllImport(dllFullName)]
        public static extern IntPtr GetFPGAInfo();
        [DllImport(dllFullName)]
        public static extern int GetSampleType();
        [DllImport(dllFullName)]
        public static extern IntPtr GetMac();
        [DllImport(dllFullName)]
        public static extern int GetSystem7T();

        [DllImport(dllFullName)]
        public static extern int C_InitClientInterface();

        [DllImport(dllFullName)]
        public static extern void C_UninitClientInterface();

        [DllImport(dllFullName)]
        public static extern int C_GetConnectStatus(int boxType);

        [DllImport(dllFullName)]
        public static extern int GetConnectStatus(int boxType);

        [DllImport(dllFullName)]
        public static extern void C_SetVerboseLevel(int vlevel);

        [DllImport(dllFullName)]
        public static extern void SetVerboseLevel(int vlevel);

        [DllImport(dllFullName)]
        public static extern int SetParameterFile(string filename, bool isedit);

        [DllImport(dllFullName)]
        public static extern int C_SetParameterFile(string filename, bool isedit);

        [DllImport(dllFullName)]
        public static extern int parse_boxinf(string filename);

        [DllImport(dllFullName)]
        public static extern int Init(string initfile);

        [DllImport(dllFullName)]
        public static extern void SetChannelValue(shimChannel channel, float value);

        [DllImport(dllFullName)]
        //        public static extern int SetGradientAllScale(shimChannel channel, List<float> bufIn, int len);
        public static extern int SetGradientAllScale(shimChannel channel, float[] bufIn, int len);

        [DllImport(dllFullName)]
        public static extern int SetAllMaxtrixValue(float[,] bufIn, int len);

        [DllImport(dllFullName)]
        public static extern int SetGradientWave(string filename);


        [DllImport(dllFullName)]//int channel, float *bufIn, int len
        //public static extern int SetTxFreOffsetTable(int channel, List<float> bufIn ,int len);
        public static extern int SetTxFreOffsetTable(int channel, float[] bufIn, int len);

        [DllImport(dllFullName)]
        //public static extern int SetTxPhaseTable(int channel, List<float> bufIn, int len);
        public static extern int SetTxPhaseTable(int channel, float[] bufIn, int len);

        [DllImport(dllFullName)]
        //public static extern int SetTxGainTable(int channel, List<float> bufIn, int len);
        public static extern int SetTxGainTable(int channel, float[] bufIn, int len);

        [DllImport(dllFullName)]
        public static extern int SetTxCenterFre(boxType box, int boardno, int channel, double freq);

        [DllImport(dllFullName)]
        public static extern int C_SetTxCenterFre(boxType box, boardType boardno, Channel channel, double freq);

        [DllImport(dllFullName)]
        public static extern int SetRFWaves(string wavename);

        [DllImport(dllFullName)]
        public static extern int SetRxBW(boxType box, boardType boardno, Channel channel, int bandwith);

        [DllImport(dllFullName)]
        public static extern int SetRxFreOffsetTable(boxType box, boardType boardno, Channel channel, string filename);


        [DllImport(dllFullName)]
        public static extern int SetRxPhaseTable(boxType box, boardType boardno, Channel channel, string filename);

        [DllImport(dllFullName)]
        public static extern int SetRxCenterFre(boxType box, boardType boardno, Channel channel, double freq, bool isAllSet);

        [DllImport(dllFullName)]
        public static extern int C_SetRxCenterFre(boxType box, boardType boardno, Channel channel, double freq, bool isAllSet);

        [DllImport(dllFullName)]
        public static extern int SetAllPreempValue();

        [DllImport(dllFullName)]
        public static extern void SetPreempValue(shimChannel channel, int keys, float value);

        [DllImport(dllFullName)]
        //public static extern float GetPreempValue(shimChannel channel, PreempKeys keys);
        public static extern float GetPreempValue(shimChannel channel, int keys);

        [DllImport(dllFullName)]
        public static extern float GetChannelValue(shimChannel channel);

        [DllImport(dllFullName)]
        public static extern void Abort();

        [DllImport(dllFullName)]
        public static extern void C_Abort();

        [DllImport(dllFullName)]
        public static extern void Pause();

        [DllImport(dllFullName)]
        public static extern void C_Pause();

        [DllImport(dllFullName)]
        public static extern void Continue();

        [DllImport(dllFullName)]
        public static extern int Run();

        [DllImport(dllFullName)]
        public static extern int C_Run();

        [DllImport(dllFullName)]
        public static extern int SetupModeRun();

        [DllImport(dllFullName)]
        public static extern int C_SetupModeRun();

        [DllImport(dllFullName)]
        public static extern int SetOutputPath(string path);

        [DllImport(dllFullName)]
        public static extern int C_SetOutputPath(string filename);

        [DllImport(dllFullName)]
        public static extern int SetOutputPrefix(string prefix);

        [DllImport(dllFullName)]
        public static extern string GetOutputFile();
        [DllImport(dllFullName)]
        public static extern int SetParameter(string name, double value);

        [DllImport(dllFullName)]
        public static extern int C_SetParameter(string name, double value);

        [DllImport(dllFullName)]
        public static extern double GetParameter(string name);

        [DllImport(dllFullName)]
        public static extern int SetParameterArray(string name, double[] bufIn, int len);

        [DllImport(dllFullName)]
        public static extern double C_GetParameter(string name);

        [DllImport(dllFullName)]
        public static extern void CloseSys();

        //for test
        [DllImport(dllFullName)]
        public static extern void ReceiveEnvironmentClear();

        [DllImport(dllFullName)]
        public static extern void ReceiveEnvironmentSet();

        //HW config
        [DllImport(dllFullName)]
        public static extern int ConfigFile(string filename);

        [DllImport(dllFullName)]
        public static extern int SetScanLines(int lines);

        [DllImport(dllFullName)]
        public static extern int GetScanLines();

        [DllImport(dllFullName)]
        public static extern int C_SetScanLines(int lines);

        [DllImport(dllFullName)]
        public static extern int C_GetScanLines();

        [DllImport(dllFullName)]
        public static extern int SetTxATT(boxType box, boardType board, Channel channel, float value);

        [DllImport(dllFullName)]
        public static extern int SetRxATT(boxType box, boardType board, Channel channel, float att, float amp1, float amp2, float amp3, int switchValue);

        [DllImport(dllFullName)]
        public static extern int SaveTXCaliValue(boxType box, boardType board);

        [DllImport(dllFullName)]
        public static extern int SaveTXAllCaliValue(boxType box);

        [DllImport(dllFullName)]
        public static extern int SaveParameterFile(string filename);

        [DllImport(dllFullName)]
        public static extern int ConfigSingleReg(boxType box, int addr, int value);

        [DllImport(dllFullName)]
        public static extern void RegisterPrintStr(sendDLLInfo callback);

        [DllImport(dllFullName)]
        public static extern void C_RegisterPrintStr(sendDLLInfo callback);

        [DllImport(dllFullName)]
        public static extern void ArmExit();

        [DllImport(dllFullName)]
        public static extern int QueryReg(boxType box, int addr, int chipSel);

        [DllImport(dllFullName)]
        public static extern void RegisterImageP(sendString callback);

        [DllImport(dllFullName)]
        public static extern void C_RegisterImageP(sendString callback);

        [DllImport(dllFullName)]
        public static extern void RegisterAbnormal(sendAbnormal callback);

        [DllImport(dllFullName)]
        public static extern IntPtr GetDLLVersion();

        [DllImport(dllFullName)]
        public static extern IntPtr GetArmVer();

        [DllImport(dllFullName)]
        public static extern IntPtr GetArmDriveVer();
        [DllImport(dllFullName)]
        public static extern IntPtr GetKernelVer();

        [DllImport(dllFullName)]
        public static extern int ScanCompleted();

        [DllImport(dllFullName)]
        public static extern int C_ScanCompleted();

        [DllImport(dllFullName)]
        public static extern IntPtr GetSlotInfo(int boxType, int slot, int real);

        [DllImport(dllFullName)]
        public static extern void C_RegisterDataRecv(sendData callback);

        [DllImport(dllFullName)]
        public static extern void RegisterDataRecv(sendData callback);

        [DllImport(dllFullName)]
        public static extern int GetLastErr();

        [DllImport(dllFullName)]
        public static extern float GetCenterFre();

        [DllImport(dllFullName)]
        public static extern float C_GetCenterFre();

        //[DllImport(dllFullName)]
        //public static extern int SetRxCenterFre(boxType box, boardType boardno, Channel channel, float freq, bool isAllSet);
        [DllImport(dllFullName)]
        public static extern int SetFilterType(int type);

        [DllImport(dllFullName)]
        public static extern int C_PrepareRun();

        [DllImport(dllFullName)]
        public static extern int C_SetRxFilter(double samplingPeriod);

        [DllImport(dllFullName)]
        public static extern int C_SeqStart();

        [DllImport(dllFullName)]
        public static extern void SetSaveModePilot(int pilotSaveMode);

        [DllImport(dllFullName)]
        public static extern void SetSaveMode(int SaveMode);

        [DllImport(dllFullName)]
        public static extern int SingleSample(int box, int boardno, int channel, int samplePoints);


        [DllImport(dllFullName)]
        public static extern int UpdateFPGA(int bx, int slot, string filename);


        [DllImport(dllFullName)]
        public static extern int GetUpdataSatus(int box);


        [DllImport(dllFullName)]
        public static extern int GetUpdataStyle(int box);

        [DllImport(dllFullName)]
        public static extern int SetChannelValid(string value);

        [DllImport(dllFullName)]
        public static extern int SaveRXCaliValue(int box, boardType board);

        [DllImport(dllFullName)]
        public static extern int UpdateArmExe(string fileName);

        [DllImport(dllFullName)]
        public static extern int UpdateArmDrive(string fileName);

        [DllImport(dllFullName)]
        public static extern IntPtr GetRxAttInfo();

        [DllImport(dllFullName)]
        public static extern IntPtr GetTxAttInfo();

        [DllImport(dllFullName)]
        public static extern double GetSystemFreq();

        [DllImport(dllFullName)]
        public static extern int GetTotalChannel();

        [DllImport(dllFullName)]
        public static extern int SaveSystemFile();

        [DllImport(dllFullName)]
        public static extern void SetTotalChannel(int value);

        [DllImport(dllFullName)]
        public static extern IntPtr LoadPreempValue();


        [DllImport(dllFullName)]
        public static extern float GetSingleGraGmax(int channel);

        [DllImport(dllFullName)]
        public static extern int SetSingleGraGmax(int channel, float value);

        [DllImport(dllFullName)]
        public static extern IntPtr GetChannelValid();

        [DllImport(dllFullName)]
        public static extern int GetSingleChannelValid(int channel);

        [DllImport(dllFullName)]
        // public static extern int SetRxGainTable(List<float> bufIn, int len);
        public static extern int SetRxGainTable(float[] bufIn, int len);

        [DllImport(dllFullName)]
        //public static extern int SetRxPhaseTable(List<float> bufIn, int len);
        public static extern int SetRxPhaseTable(float[] bufIn, int len);

        [DllImport(dllFullName)]
        // public static extern int SetRxFreOffsetTable(List<float> bufIn, int len);
        public static extern int SetRxFreOffsetTable(float[] bufIn, int len);

        [DllImport(dllFullName)]
        public static extern int GetTotalScanNo();

        [DllImport(dllFullName)]
        public static extern int GetCurrentScanNo();

        [DllImport(dllFullName)]
        public static extern int SetSingleGraAnalogDelay(int channel, int delay);

        [DllImport(dllFullName)]
        public static extern int SetAllGraAnalogDelay();

        [DllImport(dllFullName)]
        public static extern int GetGraAnalogDelay(int channel);
        [DllImport(dllFullName)]
        public static extern int SetRxCompGainTable(string fileName);
        [DllImport(dllFullName)]
        public static extern int SetGradInf(string fileName);
        [DllImport(dllFullName)]
        public static extern int SetADC(string fileName, int board);
        [DllImport(dllFullName)]
        public static extern void SaveQueryPath(string fileName);
        [DllImport(dllFullName)]
        public static extern int QueryRxCompGainTable(int channel);
        [DllImport(dllFullName)]
        public static extern int QueryADC(int board);
        [DllImport(dllFullName)]
        public static extern int QueryGradInfo();
        [DllImport(dllFullName)]
        public static extern int Set9520Stp(int board, string fileName);
        [DllImport(dllFullName)]
        public static extern int Query9520Stp();
        [DllImport(dllFullName)]
        public static extern int SetAuxReg(string fileName);
        [DllImport(dllFullName)]
        public static extern int QueryAuxReg();
        [DllImport(dllFullName)]
        public static extern void SetAverageMode(int averageMode);
        [DllImport(dllFullName)]
        public static extern int ParseGradInf(string filename);
        [DllImport(dllFullName)]
        public static extern int SetADCToFPGA(string filename, int boardno);
        [DllImport(dllFullName)]
        public static extern void ConfigAuxReg(string filename);
        [DllImport(dllFullName)]
        public static extern int SetRxCompGainTableTest(string filename);
        [DllImport(dllFullName)]
        public static extern int Parse9520Stp(int board, string fileName);
        [DllImport(dllFullName)]
        public static extern IntPtr GetAdcFileName();
        [DllImport(dllFullName)]
        public static extern double GetTemperature();
        [DllImport(dllFullName)]
        public static extern void SetUpdateEveryView(int value);

        [DllImport(dllFullName)]
        public static extern int GetUpdateEveryView();

    }

    public enum boardType
    {
        MAIN_BOARD,
        GRADP,
        GRADR,
        GRADS,
        TX1,
        TX2,
        RX1,
        RX2,
        RX3,
        RX4
    }
    public enum PreempKeys
    {
        A1,
        A2,
        A3,
        A4,
        A5,
        A6,
        T1,
        T2,
        T3,
        T4,
        T5,
        T6,
        A1X,
        A2X,
        A3X,
        T1X,
        T2X,
        T3X,
        A1Y,
        A2Y,
        A3Y,
        T1Y,
        T2Y,
        T3Y,
        A1Z,
        A2Z,
        A3Z,
        T1Z,
        T2Z,
        T3Z
    }
    public enum shimChannel
    {
        CHANNEL_X,
        CHANNEL_Y,
        CHANNEL_Z,
        CHANNEL_B0,
        CHANNEL_YX,
        CHANNEL_XY,
        CHANNEL_XZ,
        CHANNEL_ZX,
        CHANNEL_ZY,
        CHANNEL_YZ,
        CHANNEL_LB0

    }
    public enum boxType
    {
        M,
        E1,
        E2,
        E3,
    }
    public enum Channel
    {
        C1,
        C2,
        C3,
        C4,
        C5,
        C6,
        C7,
        C8
    }

    public delegate void sendString(string fileNameString);
    public delegate void sendDLLInfo(int level, string info);
    public delegate void sendAbnormal(int boxtype, string info);
    public delegate void sendData(IntPtr pData, int size);
}
