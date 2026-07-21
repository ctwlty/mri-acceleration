using System;

namespace testDLL
{
    public enum DataType { Real, Comp };
    class RealTimeData
    {
        public double[,] data;//对于复数数据，每个数据依次为实部、虚部。第一维是通道
        public double[] tdX;
        //  public double[,] spectrum;
        public int[] channel;
        //  public double[] sdX;
        public int[] tags;
        public byte box;
        public byte noChannels;
        public int viewsPerSeg;
        public int noSamples;
        public DataType dataType;
        public int buildTag = 0;
        //   public bool needUpdate = false;
        //   private static int seqCnt = 0;
        private static DataType dataTypePre;

        public int batchSlices = 1;

        public RealTimeData()
        {
        }
        public void RealTimeDataProcess(byte[] dataStream)
        {
            //needUpdate = false;
            if (dataStream[1] == 0)
            {
                dataType = DataType.Comp;
            }
            else if (dataStream[1] == 1)
            {
                dataType = DataType.Real;
                return; //20200512添加，非双路数据时不在分析，避免数据类型切换引起的异常
            }
            else
            {
                Console.WriteLine("Unknown data type!");
                return;   //20200512添加，非双路数据时不在分析，避免数据类型切换引起的异常
            }

            if (buildTag == 0)//首次调用时赋值
            {
                dataTypePre = dataType;
            }
            if (dataTypePre != dataType)//若传入的数据类型有变化，复位buildTag.
            {
                buildTag = 0;
                dataTypePre = dataType;
            }

            box = dataStream[0];

            batchSlices = dataStream[17];
            //viewsPerSeg = dataStream[3];// 
            noChannels = dataStream[2];
            byte[] tmpBytes = new byte[4];
            Buffer.BlockCopy(dataStream, 3, tmpBytes, 0, 4);
            viewsPerSeg = BitConverter.ToInt32(tmpBytes, 0);
            bytesSwap(tmpBytes);

            Buffer.BlockCopy(dataStream, 7, tmpBytes, 0, 4);

            noSamples = BitConverter.ToInt32(tmpBytes, 0);
            tags = new int[noChannels];
            channel = new int[noChannels];//存放通道号。

            if (buildTag == 0)
            {
                tdX = new double[viewsPerSeg * noSamples * batchSlices];
                if (dataType == DataType.Comp)
                {
                    data = new double[noChannels, 2 * viewsPerSeg * noSamples * batchSlices];
                }
                else
                {
                    data = new double[noChannels, viewsPerSeg * noSamples * batchSlices];
                }
            }
            buildTag = 1;

            if (buildTag == 0)
            {
                return;
            }

            if (dataType == DataType.Comp)
            {
                for (int i = 0; i < tdX.Length; i++)
                {
                    tdX[i] = i;//% noSamples;// i;
                }
            }
            int startAt = 19;
            if (dataType == DataType.Comp)
            {
                for (int i = 0; i < noChannels; i++)
                {
                    //DLL通道数从1开始。
                    tags[i] = dataStream[startAt + i * (6 * viewsPerSeg * noSamples * batchSlices + 1)] - 1;

                    for (int j = 0; j < 2 * noSamples * viewsPerSeg * batchSlices; j++)
                    {
                        Buffer.BlockCopy(dataStream, startAt + 1 + i * (6 * viewsPerSeg * noSamples * batchSlices + 1) + 3 * j, tmpBytes, 0, 3);

                        if (tmpBytes[2] >= 128)
                        {
                            tmpBytes[3] = 255;
                        }
                        else
                        {
                            tmpBytes[3] = 0;
                        }
                        data[i, j] = BitConverter.ToInt32(tmpBytes, 0);
                    }
                    channel[i] = tags[i];
                }

            }
            else
            {
                // MessageBox.Show("Not implemented!");
            }
        }
        private void bytesSwap(byte[] Bytes)
        {
            byte tmpByte;
            int len;
            len = Bytes.Length;
            for (int i = 0; i < len / 2; i++)
            {
                tmpByte = Bytes[len - i - 1];
                Bytes[len - i - 1] = Bytes[i];
                Bytes[i] = tmpByte;
            }
        }
    }
}
