#include <string.h>
#include "unity.h"
#include "sensor_math.h"

/*
 * Calibration used by the datasheet example of the Bosch BMP280 (the BME280 uses the
 * same temperature and pressure formulas). Humidity coefficients are typical values.
 */
static Bme280Calib example_calib(void)
{
    Bme280Calib c;
    memset(&c, 0, sizeof c);
    c.dig_T1 = 27504; c.dig_T2 = 26435; c.dig_T3 = -1000;
    c.dig_P1 = 36477; c.dig_P2 = -10685; c.dig_P3 = 3024; c.dig_P4 = 2855;
    c.dig_P5 = 140;   c.dig_P6 = -7;     c.dig_P7 = 15500; c.dig_P8 = -14600;
    c.dig_P9 = 6000;
    c.dig_H1 = 75; c.dig_H2 = 362; c.dig_H3 = 0; c.dig_H4 = 315; c.dig_H5 = 50; c.dig_H6 = 30;
    return c;
}

void setUp(void) {}
void tearDown(void) {}

/* ---------- floating point reference (Bosch datasheet, "double" version) ---------- */

static double ref_tfine(const Bme280Calib *c, int32_t adc_T)
{
    double var1 = (adc_T / 16384.0 - c->dig_T1 / 1024.0) * c->dig_T2;
    double d = adc_T / 131072.0 - c->dig_T1 / 8192.0;
    double var2 = d * d * c->dig_T3;
    return var1 + var2;
}

static double ref_press(const Bme280Calib *c, int32_t adc_P, double t_fine)
{
    double var1 = t_fine / 2.0 - 64000.0;
    double var2 = var1 * var1 * c->dig_P6 / 32768.0;
    var2 = var2 + var1 * c->dig_P5 * 2.0;
    var2 = var2 / 4.0 + c->dig_P4 * 65536.0;
    var1 = (c->dig_P3 * var1 * var1 / 524288.0 + c->dig_P2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * c->dig_P1;
    double p = 1048576.0 - adc_P;
    p = (p - var2 / 4096.0) * 6250.0 / var1;
    var1 = c->dig_P9 * p * p / 2147483648.0;
    var2 = p * c->dig_P8 / 32768.0;
    return p + (var1 + var2 + c->dig_P7) / 16.0;
}

static double ref_hum(const Bme280Calib *c, int32_t adc_H, double t_fine)
{
    double v = t_fine - 76800.0;
    v = (adc_H - (c->dig_H4 * 64.0 + c->dig_H5 / 16384.0 * v)) *
        (c->dig_H2 / 65536.0 * (1.0 + c->dig_H6 / 67108864.0 * v *
                                (1.0 + c->dig_H3 / 67108864.0 * v)));
    v = v * (1.0 - c->dig_H1 * v / 524288.0);
    if (v > 100.0) v = 100.0;
    if (v < 0.0) v = 0.0;
    return v;
}

/* ---------- BME280: parsing ---------- */

void test_bme280_parse_calibration_registers(void)
{
    uint8_t b[26] = {0}, h[7] = {0};
    Bme280Calib c;

    b[0] = 0x70; b[1] = 0x6B;        /* T1 = 0x6B70 = 27504 */
    b[2] = 0x43; b[3] = 0x67;        /* T2 = 0x6743 = 26435 */
    b[4] = 0x18; b[5] = 0xFC;        /* T3 = 0xFC18 = -1000 (signed) */
    b[22] = 0x70; b[23] = 0x17;      /* P9 = 0x1770 = 6000 */
    b[25] = 75;                      /* H1 */
    h[0] = 0x6A; h[1] = 0x01;        /* H2 = 0x016A = 362 */
    h[2] = 0;                        /* H3 */
    h[3] = 0x13; h[4] = 0xA7; h[5] = 0x03; /* H4/H5 share register 0xE5 */
    h[6] = 0x1E;                     /* H6 = 30 */

    Bme280_ParseCalib(b, h, &c);

    TEST_ASSERT_EQUAL_UINT16(27504, c.dig_T1);
    TEST_ASSERT_EQUAL_INT16(26435, c.dig_T2);
    TEST_ASSERT_EQUAL_INT16(-1000, c.dig_T3);
    TEST_ASSERT_EQUAL_INT16(6000, c.dig_P9);
    TEST_ASSERT_EQUAL_UINT8(75, c.dig_H1);
    TEST_ASSERT_EQUAL_INT16(362, c.dig_H2);
    /* H4 = 0xE4 << 4 | 0xE5[3:0] = 0x13 * 16 + 0x7 = 311 */
    TEST_ASSERT_EQUAL_INT16(311, c.dig_H4);
    /* H5 = 0xE6 << 4 | 0xE5[7:4] = 0x03 * 16 + 0xA = 58 */
    TEST_ASSERT_EQUAL_INT16(58, c.dig_H5);
    TEST_ASSERT_EQUAL_INT8(30, c.dig_H6);
}

void test_bme280_parse_calibration_negative_h4_h6(void)
{
    uint8_t b[26] = {0}, h[7] = {0};
    Bme280Calib c;

    h[3] = 0xFF; h[4] = 0x05; h[5] = 0xFE; /* H4 = -1*16 | 5, H5 = -2*16 | 0 */
    h[6] = 0xE2;                           /* H6 = -30 */
    Bme280_ParseCalib(b, h, &c);

    TEST_ASSERT_EQUAL_INT16(-16 | 5, c.dig_H4);
    TEST_ASSERT_EQUAL_INT16(-32 | 0, c.dig_H5);
    TEST_ASSERT_EQUAL_INT8(-30, c.dig_H6);
}

void test_bme280_parse_raw_data(void)
{
    /* press = 0x65 5A C0 -> 20 bits 0x655AC, temp = 0x7E D1 00 -> 0x7ED10, hum = 0x8000 */
    const uint8_t buf[8] = { 0x65, 0x5A, 0xC0, 0x7E, 0xD1, 0x00, 0x80, 0x00 };
    int32_t t, p, h;

    Bme280_ParseRaw(buf, &t, &p, &h);

    TEST_ASSERT_EQUAL_INT32(0x655AC, p);
    TEST_ASSERT_EQUAL_INT32(0x7ED10, t);
    TEST_ASSERT_EQUAL_INT32(0x8000, h);
}

/* ---------- BME280: compensation ---------- */

void test_bme280_datasheet_example_temperature_and_pressure(void)
{
    Bme280Calib c = example_calib();
    Bme280Result r;

    Bme280_Compensate(&c, 519888, 415148, 30000, &r);

    /* BMP280 datasheet example: 25.08 degC and 100653.27 Pa */
    TEST_ASSERT_EQUAL_INT32(2508, r.temp_centi);
    TEST_ASSERT_INT32_WITHIN(1, 100653, (int32_t)r.press_pa);
}

void test_bme280_matches_float_reference_over_realistic_range(void)
{
    Bme280Calib c = example_calib();
    int32_t max_dt = 0, max_dp = 0, max_dh = 0;

    for (int32_t t = 440000; t <= 560000; t += 4000) {
        for (int32_t p = 380000; p <= 450000; p += 5000) {
            for (int32_t h = 20000; h <= 40000; h += 4000) {
                Bme280Result r;
                Bme280_Compensate(&c, t, p, h, &r);

                double tf = ref_tfine(&c, t);
                double dt = r.temp_centi - tf / 51.20;      /* centi-degC */
                double dp = r.press_pa - ref_press(&c, p, tf);
                double dh = r.hum_centi - ref_hum(&c, h, tf) * 100.0;

                if (dt < 0) dt = -dt;
                if (dp < 0) dp = -dp;
                if (dh < 0) dh = -dh;
                if ((int32_t)dt > max_dt) max_dt = (int32_t)dt;
                if ((int32_t)dp > max_dp) max_dp = (int32_t)dp;
                if ((int32_t)dh > max_dh) max_dh = (int32_t)dh;
            }
        }
    }
    TEST_ASSERT_LESS_OR_EQUAL_INT32(1, max_dt);    /* 0.01 degC */
    TEST_ASSERT_LESS_OR_EQUAL_INT32(1, max_dp);    /* 1 Pa */
    TEST_ASSERT_LESS_OR_EQUAL_INT32(1, max_dh);    /* 0.01 %RH */
}

void test_bme280_temperature_increases_with_raw_value(void)
{
    Bme280Calib c = example_calib();
    Bme280Result lo, hi;

    Bme280_Compensate(&c, 500000, 415148, 30000, &lo);
    Bme280_Compensate(&c, 520000, 415148, 30000, &hi);

    TEST_ASSERT_GREATER_THAN_INT32(lo.temp_centi, hi.temp_centi);
}

void test_bme280_pressure_decreases_with_raw_value(void)
{
    Bme280Calib c = example_calib();
    Bme280Result lo, hi;

    Bme280_Compensate(&c, 519888, 400000, 30000, &lo);
    Bme280_Compensate(&c, 519888, 430000, 30000, &hi);

    /* A higher raw ADC value means a lower pressure for this sensor family. */
    TEST_ASSERT_GREATER_THAN_UINT32(hi.press_pa, lo.press_pa);
}

void test_bme280_pressure_is_zero_when_p1_is_zero(void)
{
    Bme280Calib c = example_calib();
    Bme280Result r;

    c.dig_P1 = 0;  /* would divide by zero */
    Bme280_Compensate(&c, 519888, 415148, 30000, &r);

    TEST_ASSERT_EQUAL_UINT32(0, r.press_pa);
}

void test_bme280_humidity_is_clamped_to_0_and_100_percent(void)
{
    Bme280Calib c = example_calib();
    Bme280Result r;

    Bme280_Compensate(&c, 519888, 415148, 0, &r);
    TEST_ASSERT_EQUAL_UINT32(0, r.hum_centi);

    Bme280_Compensate(&c, 519888, 415148, 65535, &r);
    TEST_ASSERT_EQUAL_UINT32(10000, r.hum_centi);
}

/* ---------- MPU6050 ---------- */

void test_mpu6050_parse_raw_big_endian_and_signed(void)
{
    const uint8_t buf[14] = {
        0x40, 0x00,   /* ax =  16384 */
        0xC0, 0x00,   /* ay = -16384 */
        0x00, 0x01,   /* az =  1 */
        0xFF, 0xFF,   /* temp = -1 */
        0x00, 0x83,   /* gx = 131 */
        0xFF, 0x7D,   /* gy = -131 */
        0x80, 0x00    /* gz = -32768 */
    };
    Mpu6050Raw r;

    Mpu6050_ParseRaw(buf, &r);

    TEST_ASSERT_EQUAL_INT16(16384, r.accel_x);
    TEST_ASSERT_EQUAL_INT16(-16384, r.accel_y);
    TEST_ASSERT_EQUAL_INT16(1, r.accel_z);
    TEST_ASSERT_EQUAL_INT16(-1, r.temp_raw);
    TEST_ASSERT_EQUAL_INT16(131, r.gyro_x);
    TEST_ASSERT_EQUAL_INT16(-131, r.gyro_y);
    TEST_ASSERT_EQUAL_INT16(-32768, r.gyro_z);
}

void test_mpu6050_accel_one_g_is_1000_mg(void)
{
    const Mpu6050Raw raw = { .accel_x = 0, .accel_y = 0, .accel_z = 16384 };
    const int32_t off[3] = { 0, 0, 0 };
    Mpu6050Scaled s;

    Mpu6050_Scale(&raw, off, &s);

    TEST_ASSERT_EQUAL_INT32(0, s.accel_mg_x);
    TEST_ASSERT_EQUAL_INT32(0, s.accel_mg_y);
    TEST_ASSERT_EQUAL_INT32(1000, s.accel_mg_z);
}

void test_mpu6050_accel_negative_values(void)
{
    const Mpu6050Raw raw = { .accel_x = -16384, .accel_y = -8192 };
    const int32_t off[3] = { 0, 0, 0 };
    Mpu6050Scaled s;

    Mpu6050_Scale(&raw, off, &s);

    TEST_ASSERT_EQUAL_INT32(-1000, s.accel_mg_x);
    TEST_ASSERT_EQUAL_INT32(-500, s.accel_mg_y);
}

void test_mpu6050_accel_full_scale_does_not_overflow(void)
{
    const Mpu6050Raw raw = { .accel_x = 32767, .accel_y = -32768 };
    const int32_t off[3] = { 0, 0, 0 };
    Mpu6050Scaled s;

    Mpu6050_Scale(&raw, off, &s);

    TEST_ASSERT_EQUAL_INT32(1999, s.accel_mg_x);   /* 32767 * 1000 / 16384 */
    TEST_ASSERT_EQUAL_INT32(-2000, s.accel_mg_y);
}

void test_mpu6050_gyro_131_lsb_is_one_dps(void)
{
    const Mpu6050Raw raw = { .gyro_x = 131, .gyro_y = -131, .gyro_z = 0 };
    const int32_t off[3] = { 0, 0, 0 };
    Mpu6050Scaled s;

    Mpu6050_Scale(&raw, off, &s);

    TEST_ASSERT_EQUAL_INT32(100, s.gyro_cdps_x);    /* 1.00 deg/s */
    TEST_ASSERT_EQUAL_INT32(-100, s.gyro_cdps_y);
    TEST_ASSERT_EQUAL_INT32(0, s.gyro_cdps_z);
}

void test_mpu6050_gyro_offset_is_subtracted(void)
{
    const Mpu6050Raw raw = { .gyro_x = 150, .gyro_y = -40, .gyro_z = 7 };
    const int32_t off[3] = { 150, -40, 7 };
    Mpu6050Scaled s;

    Mpu6050_Scale(&raw, off, &s);

    TEST_ASSERT_EQUAL_INT32(0, s.gyro_cdps_x);
    TEST_ASSERT_EQUAL_INT32(0, s.gyro_cdps_y);
    TEST_ASSERT_EQUAL_INT32(0, s.gyro_cdps_z);
}

void test_mpu6050_gyro_full_scale_is_about_250_dps(void)
{
    const Mpu6050Raw raw = { .gyro_x = 32767, .gyro_y = -32768 };
    const int32_t off[3] = { 0, 0, 0 };
    Mpu6050Scaled s;

    Mpu6050_Scale(&raw, off, &s);

    TEST_ASSERT_INT32_WITHIN(10, 25012, s.gyro_cdps_x);   /* 32767 / 131 = 250.12 deg/s */
    TEST_ASSERT_INT32_WITHIN(10, -25012, s.gyro_cdps_y);
}

void test_mpu6050_temperature_formula(void)
{
    const int32_t off[3] = { 0, 0, 0 };
    Mpu6050Raw raw = { .temp_raw = 0 };
    Mpu6050Scaled s;

    Mpu6050_Scale(&raw, off, &s);
    TEST_ASSERT_EQUAL_INT32(3653, s.temp_centi);          /* 36.53 degC at raw 0 */

    raw.temp_raw = -3400;                                  /* -10 degC offset */
    Mpu6050_Scale(&raw, off, &s);
    TEST_ASSERT_EQUAL_INT32(2653, s.temp_centi);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bme280_parse_calibration_registers);
    RUN_TEST(test_bme280_parse_calibration_negative_h4_h6);
    RUN_TEST(test_bme280_parse_raw_data);
    RUN_TEST(test_bme280_datasheet_example_temperature_and_pressure);
    RUN_TEST(test_bme280_matches_float_reference_over_realistic_range);
    RUN_TEST(test_bme280_temperature_increases_with_raw_value);
    RUN_TEST(test_bme280_pressure_decreases_with_raw_value);
    RUN_TEST(test_bme280_pressure_is_zero_when_p1_is_zero);
    RUN_TEST(test_bme280_humidity_is_clamped_to_0_and_100_percent);
    RUN_TEST(test_mpu6050_parse_raw_big_endian_and_signed);
    RUN_TEST(test_mpu6050_accel_one_g_is_1000_mg);
    RUN_TEST(test_mpu6050_accel_negative_values);
    RUN_TEST(test_mpu6050_accel_full_scale_does_not_overflow);
    RUN_TEST(test_mpu6050_gyro_131_lsb_is_one_dps);
    RUN_TEST(test_mpu6050_gyro_offset_is_subtracted);
    RUN_TEST(test_mpu6050_gyro_full_scale_is_about_250_dps);
    RUN_TEST(test_mpu6050_temperature_formula);
    return UNITY_END();
}
