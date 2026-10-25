# STM32 Sensor Data Logger

Firmware για board STM32 Nucleo-64 (NUCLEO-F401RE) που διαβάζει τον αισθητήρα περιβάλλοντος BME280 και το IMU MPU6050 μέσω I2C, εμφανίζει τις μετρήσεις σε οθόνη OLED SSD1306 και προσφέρει διαδραστικό command interface πάνω από UART. Γραμμένο σε C με το STM32 HAL, με κάθε περιφερειακό σε δικό του driver module.

Είναι project εκμάθησης και portfolio που χτίζεται βήμα-βήμα. Η ενότητα "Τρέχουσα κατάσταση" δείχνει τι δουλεύει σήμερα και τι έχει μείνει.

## Hardware

| Εξάρτημα | Ρόλος | Διεύθυνση I2C (7-bit) |
|---|---|---|
| NUCLEO-F401RE (ARM Cortex-M4) | Κύριο board, ενσωματωμένος ST-LINK debugger και virtual COM port | |
| BME280 (module HW-611) | Θερμοκρασία, υγρασία, πίεση | `0x76` |
| MPU6050 | Επιταχυνσιόμετρο και γυροσκόπιο 3 αξόνων | `0x68` |
| SSD1306 OLED 128x64 | Τοπική οθόνη μετρήσεων | `0x3C` |
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
  Inc/   main.h, command_menu.h, bme280.h, mpu6050.h,
         ssd1306.h, ssd1306_font.h, display_task.h
  Src/   main.c, command_menu.c, bme280.c, mpu6050.c,
         ssd1306.c, display_task.c
Drivers/            (παράγονται αυτόματα: STM32 HAL και CMSIS)
docs/               (screenshots και captures)
*.ioc               (ρυθμίσεις STM32CubeMX)
```

Κάθε αισθητήρας και η οθόνη έχουν δικό τους driver. Το `display_task` είναι ο "εφαρμογικός" κώδικας που διαβάζει τους αισθητήρες και ζωγραφίζει την οθόνη, χωρίς να μπλοκάρει το κύριο loop.

## Τρέχουσα κατάσταση

- [x] Toolchain: STM32CubeMX 6.18.1 + STM32CubeIDE 2.2.0
- [x] Bring-up του NUCLEO-F401RE (flash, debug, virtual COM port)
- [x] UART command interface (RX με interrupts, `printf` προς USART2)
- [x] Driver BME280: ανάγνωση calibration, compensation σε θερμοκρασία, πίεση και υγρασία
- [x] Driver MPU6050: ρύθμιση ranges και φίλτρου, κλιμάκωση σε g και °/s, βαθμονόμηση offset γυροσκοπίου
- [x] Εντολή scan του I2C bus
- [x] Επαλήθευση της κίνησης στο I2C με logic analyzer (BME280 και MPU6050)
- [x] Driver SSD1306 (framebuffer, γραμματοσειρά 5x7, text rendering)
- [x] Ζωντανή εμφάνιση μετρήσεων στην οθόνη, ανανέωση κάθε 1 δευτερόλεπτο
- [ ] Καταγραφή δεδομένων με timestamps (buffer) και ανάγνωσή τους από το UART

## UART command interface (115200, 8N1)

| Εντολή | Περιγραφή |
|---|---|
| `help` | Εμφανίζει τη λίστα εντολών |
| `led on` / `led off` | Έλεγχος του LED LD2 |
| `status` | Εμφανίζει το system tick (ms) |
| `scan` | Σαρώνει το I2C bus και εμφανίζει τις διευθύνσεις που απαντούν |
| `bme` | Raw τιμές πίεσης, θερμοκρασίας και υγρασίας του BME280 |
| `env` | Θερμοκρασία (°C), πίεση (hPa) και υγρασία (%RH) |
| `mpu` | Raw τιμές επιταχυνσιομέτρου και γυροσκοπίου του MPU6050 |
| `imu` | Επιτάχυνση (mg), γωνιακή ταχύτητα (°/s) και θερμοκρασία chip |
| `mpucal` | Βαθμονόμηση του γυροσκοπίου (το board πρέπει να είναι ακίνητο) |

## Οθόνη SSD1306

Η οθόνη ανανεώνεται μία φορά το δευτερόλεπτο και δείχνει τις μετρήσεις και των δύο αισθητήρων:

![Μετρήσεις στην οθόνη SSD1306](docs/ssd1306_display.jpeg)

Διάταξη (8 γραμμές κειμένου των 21 χαρακτήρων):

```
T: 21.05 C
P: 1009.31 hPa
H: 61.18 %RH
-----------------
Accel [mg]
-41 -22 -1057
Gyro [dps]
-0.0 -0.0 -0.0
```

Ο driver κρατά ένα framebuffer 1024 bytes στη RAM. Οι συναρτήσεις σχεδίασης αλλάζουν μόνο το framebuffer, και η `SSD1306_Update()` το στέλνει στην οθόνη σελίδα-σελίδα μέσω I2C (περίπου 100 ms στα 100 kHz).

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
- Όλες οι μετρήσεις υπολογίζονται και εμφανίζονται με ακέραιους (fixed point), χωρίς `float`. Με το newlib-nano που χρησιμοποιεί το CubeIDE, το `printf("%f")` δεν υποστηρίζεται χωρίς ειδική ρύθμιση του linker.
- Ο MPU6050 είναι ρυθμισμένος στα ±2 g και ±250 °/s, με ρυθμό δειγματοληψίας 100 Hz και low-pass φίλτρο περίπου 44 Hz. Τα ranges αλλάζουν στο `MPU6050_Init()` μαζί με τις αντίστοιχες σταθερές κλιμάκωσης.
- Η θερμοκρασία του MPU6050 είναι θερμοκρασία του chip και βγαίνει συνήθως μερικούς βαθμούς πάνω από τον αέρα. Η θερμοκρασία περιβάλλοντος παίρνεται από το BME280.