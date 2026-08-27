#define SAMPLE_TIME_MS             100L
#define ENCODER_COUNTS_PER_REV     1440L
#define WHEEL_DIAMETER_X10_MM      598L
#define PI_X10000                  31416L

/*
 * 将采样周期内的编码器脉冲数换算为毫米/秒。
 * 车轮直径固定为59.8毫米。
 */
long Count_To_Speed(int count)
{
    long long numerator;
    long long denominator;

    numerator = (long long)count
              * WHEEL_DIAMETER_X10_MM
              * PI_X10000
              * 1000L;

    denominator = (long long)ENCODER_COUNTS_PER_REV
                * SAMPLE_TIME_MS
                * 10L
                * 10000L;

    return (long)(numerator / denominator);
}