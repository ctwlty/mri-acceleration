using NMRDLL;
using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace testDLL
{
    class Program
    {
        static RealTimeData RtData;
        static void Main(string[] args)
        {
            sendData FetchRealTimeData;
            RtData = new RealTimeData();

            float shimValue = 0;
            string PAR_PATH = "PTScan.par";
            string OUTPUT_PATH = "rawData";
            string OUTPUT_PREFIX = "mri_client";
            string CHANNEL_SELECT = "255";
            string INITFILE = "hw_cfg\\init.ini";
            string SAVE_PAR = "PTScan.par";

            //初始化客户端访问接口
            int stat;
            Console.WriteLine("client INIT-------------");
            stat = NMR.Init(INITFILE);
            if (stat != 0)
            {
                Console.WriteLine(" INIT ERROR......");
            }

            stat = NMR.ConfigFile(INITFILE);
            if (stat != 0)
            {
                Console.WriteLine(" CONFIG ERROR......");
            }

            NMR.SetAverageMode(0);
            NMR.SetSaveMode(1);

            //注册回调函数
            NMR.RegisterImageP(PrintSaveFile);
            FetchRealTimeData = fetchRealTimeData;
            NMR.RegisterDataRecv(FetchRealTimeData);

            //设置PAR文件
            Console.WriteLine("  SET PAR-------------");
            stat = NMR.SetParameterFile(PAR_PATH, false);
            if (stat != 0)
            {
                Console.WriteLine("  SET PAR ERROR......");
            }

            //设置数据保存文件路径
            Console.WriteLine(" SET OUTPUT PATH-------------");
            stat = NMR.SetOutputPath(OUTPUT_PATH);
            if (stat != 0)
            {
                Console.WriteLine("  SET OUTPUT PATH ERROR......");
            }

            //设置数据保存前缀
            Console.WriteLine("SET OUTPUT PRE-------------");
            stat = NMR.SetOutputPrefix(OUTPUT_PREFIX);
            if (stat != 0)
            {
                Console.WriteLine("  SET OUTPUT PRE ERROR......");
            }

            //设置通道数
            Console.WriteLine("SET CHANNEL SELECT-------------");
            stat = NMR.SetChannelValid(CHANNEL_SELECT);
            if (stat != 0)
            {
                Console.WriteLine("  SET Channel select failed......");
            }

            //设置参数
            NMR.SetParameter("viewBlock", 1);
            NMR.SetParameter("TR", 500);
            for (int i = 0; i < 8; i++)
            {
                stat = NMR.SetTxCenterFre(0, 4, i, 50);
                if (stat != 0)
                {
                    Console.WriteLine("  SET FRE failed......");
                }
            }

            //保存PAR文件
            Console.WriteLine(" SAVE PAR FILE-------------");
            stat = NMR.SaveParameterFile(SAVE_PAR);
            if (stat != 0)
            {
                Console.WriteLine("  SAVE PAR FILE failed......");
            }

            //设置匀场参数
            NMR.SetChannelValue(shimChannel.CHANNEL_X, shimValue);
            NMR.SetChannelValue(shimChannel.CHANNEL_Y, shimValue);
            NMR.SetChannelValue(shimChannel.CHANNEL_Z, shimValue);

            Console.WriteLine(" RUN-------------");
            NMR.Run();

            //查询扫描是否结束
            while ((NMR.ScanCompleted() != 0) && (NMR.ScanCompleted() != 3))
            {
                Thread.Sleep(100);
                //打印扫描状态
                Console.WriteLine(" ScanCompleted is: " + NMR.ScanCompleted().ToString());
                //获取总扫描次数
                Console.WriteLine(" GetTotalScanNo:" + NMR.GetTotalScanNo().ToString());
                //获取当前扫描次数
                Console.WriteLine(" GetCurrentScanNo:" + NMR.GetCurrentScanNo().ToString());
            }

            //终止扫描
            NMR.Abort();

            //退出系统
            NMR.CloseSys();
        }

        private static void PrintSaveFile(string str)
        {
            Console.WriteLine(" SaveFile is:" + str);
        }

        private static void fetchRealTimeData(IntPtr pData, int s)
        {
            int dataLen = s;
            byte[] data = new byte[s];

            //IntPtr src = (IntPtr)Marshal.ReadInt32(pData);//x86
            IntPtr src = (IntPtr)Marshal.ReadInt64(pData);//x64               
            Marshal.Copy(src, data, 0, dataLen);
            RtData.RealTimeDataProcess(data);//
        }
    }
}
