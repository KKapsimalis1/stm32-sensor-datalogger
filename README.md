# STM32 Sensor Data Logger

Firmware για board STM32 Nucleo-64 (NUCLEO-F401RE) που διαβάζει τον αισθητήρα περιβάλλοντος BME280 και το IMU MPU6050 μέσω I2C και προσφέρει διαδραστικό command interface πάνω από UART. Γραμμένο σε C με το STM32 HAL, με κάθε περιφερειακό σε δικό του driver module.

Είναι project εκμάθησης και portfolio που χτίζεται βήμα-βήμα. Η ενότητα "Τρέχουσα κατάσταση" δείχνει τι δουλεύει σήμερα και τι έχει μείνει.

## Hardware

| Εξάρτημα | Ρόλος | Διεύθυνση I2C (7-bit) |
|---|---|---|
| NUCLEO-F401RE (ARM Cortex-M4) | Κύριο board, ενσωματωμένος ST-LINK debugger και virtual COM port | |
| BME280 (module HW-611) | Θερμοκρασία, υγρασία, πίεση | `0x76` |
| MPU6050 | Επιταχυνσιόμετρο και γυροσκόπιο 3 αξόνων | `0x68` |
| SSD1306 OLED | Τοπική οθόνη (ο driver δεν έχει γραφτεί ακόμα) | `0x3C` |
| Logic analyzer 8 καναλιών (Saleae-compatible, 24 MHz) | Debugging πρωτοκόλλων με PulseView/sigrok | |

## Καλωδίωση

Και τα τρία modules μοιράζονται το ίδιο I2C bus (I2C1, 100 kHz, με pull-ups πάνω στα modules).

| Σήμα | Pin Nucleo | Πηγαίνει σε |
|---|---|---|
| SCL | PB8 (D15) | SCL όλων των modules |
| SDA | PB9 (D14) | SDA όλων των modules |
| 3V3 | 3.3V | VCC όλων των modules |
| GND | GND | GND όλων των modules |

Pins διευθύνσεων: BME280 `SDO` στο GND (0x76) και `CSB` στο 3V3 (λειτουργία I2C). MPU6050 `AD0` στο GND (0x68).

Άλλα pins που χρησιμοποιούνται: `LD2` στο PA5 (user LED) και USART2 στα PA2/PA3 (ST-LINK virtual COM port, 115200 8N1).

## Δομή του project

```
Core/
  Inc/   main.h, command_menu.h, bme280.h, mpu6050.h
  Src/   main.c, command_menu.c, bme280.c, mpu6050.c
Drivers/            (παράγονται αυτόματα: STM32 HAL και CMSIS)
docs/               (screenshots και captures)
*.ioc               (ρυθμίσεις STM32CubeMX)
```

## Τρέχουσα κατάσταση

- [x] Toolchain: STM32CubeMX 6.18.1 + STM32CubeIDE 2.2.0
- [x] Bring-up του NUCLEO-F401RE (flash, debug, virtual COM port)
- [x] UART command interface (RX με interrupts, `printf` προς USART2)
- [x] Driver BME280: έλεγχος chip ID, burst ανάγνωση raw πίεσης, θερμοκρασίας και υγρασίας
- [x] Driver MPU6050: wake-up, έλεγχος WHO_AM_I, burst ανάγνωση raw επιταχυνσιομέτρου και γυροσκοπίου
- [x] Εντολή scan του I2C bus
- [x] Επαλήθευση της κίνησης στο I2C με logic analyzer (BME280 και MPU6050, δες παρακάτω)
- [ ] Compensation BME280 (raw τιμές σε °C, hPa, %RH)
- [ ] Κλιμάκωση MPU6050 (raw τιμές σε g και °/s)
- [ ] Driver για την οθόνη SSD1306
- [ ] Καταγραφή δεδομένων με timestamps (buffer)

## UART command interface (115200, 8N1)

| Εντολή | Περιγραφή |
|---|---|
| `help` | Εμφανίζει τη λίστα εντολών |
| `led on` / `led off` | Έλεγχος του LED LD2 |
| `status` | Εμφανίζει το system tick (ms) |
| `scan` | Σαρώνει το I2C bus και εμφανίζει τις διευθύνσεις που απαντούν |
| `bme` | Εμφανίζει τις raw τιμές πίεσης, θερμοκρασίας και υγρασίας του BME280 |
| `mpu` | Εμφανίζει τις raw τιμές επιταχυνσιομέτρου και γυροσκοπίου του MPU6050 |

## Επαλήθευση του I2C με logic analyzer

Οι αναγνώσεις καταγράφηκαν με logic analyzer συμβατό με Saleae και αποκωδικοποιήθηκαν στο PulseView (SCL στο D0, SDA στο D1, δειγματοληψία 2 MHz).

### BME280

![Ανάγνωση των data registers του BME280 μέσω I2C](docs/i2c_bme280_read.png)

Η αποκωδικοποιημένη συναλλαγή ταιριάζει με το burst read της `BME280_ReadRaw()`:

1. Start, μετά διεύθυνση `0x76` με write bit, και ο αισθητήρας απαντά με ACK
2. Register pointer `0xF7` (αρχή των data registers)
3. Repeated start, μετά διεύθυνση `0x76` με read bit
4. Οκτώ bytes δεδομένων (πίεση 3, θερμοκρασία 3, υγρασία 2), καθένα με ACK
5. NACK στο τελευταίο byte και μετά stop

### MPU6050

![Ανάγνωση των data registers του MPU6050 μέσω I2C](docs/i2c_mpu6050_read.png)

Η συναλλαγή ταιριάζει με το burst read της `MPU6050_ReadRaw()`:

1. Start, μετά διεύθυνση `0x68` με write bit, και ο αισθητήρας απαντά με ACK
2. Register pointer `0x3B` (`ACCEL_XOUT_H`, αρχή των data registers)
3. Repeated start, μετά διεύθυνση `0x68` με read bit
4. Δεκατέσσερα bytes δεδομένων (επιτάχυνση 6, θερμοκρασία 2, γυροσκόπιο 6), καθένα με ACK
5. NACK στο τελευταίο byte και μετά stop

## Build και flash

1. Άνοιξε το αρχείο `.ioc` στο STM32CubeMX και κάνε Generate Code με toolchain STM32CubeIDE.
2. Άνοιξε το project στο STM32CubeIDE (File, Open Projects from File System).
3. Κάνε Build και μετά Debug για να γίνει flash στο board μέσω του ενσωματωμένου ST-LINK.
4. Άνοιξε serial terminal στο ST-LINK virtual COM port στα 115200 8N1 και πάτα το κουμπί RESET του board.

Σημείωση: από το STM32CubeIDE 2.0 και μετά, το STM32CubeMX είναι ξεχωριστή εφαρμογή. Οι αλλαγές σε pins και περιφερειακά γίνονται στο CubeMX και όχι μέσα στο IDE.

## Σημειώσεις

- Η πλακέτα HW-611 πουλιέται και ως BME280 και ως BMP280. Το BME280 έχει υγρασία, το BMP280 όχι. Ο driver ελέγχει το chip ID (`0x60` για BME280, `0x58` για BMP280) κατά την εκκίνηση.
- Οι raw τιμές που τυπώνει η εντολή `bme` δεν είναι ακόμα φυσικές μονάδες. Χρειάζονται τους calibration coefficients κάθε chip και τους τύπους compensation από το datasheet της Bosch.
- Οι raw τιμές της εντολής `mpu` χρειάζονται κλιμάκωση ανάλογα με το επιλεγμένο full-scale range, που δεν έχει ρυθμιστεί ακόμα (ισχύουν οι προεπιλογές του chip).