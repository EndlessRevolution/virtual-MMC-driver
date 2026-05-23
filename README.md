# Virtual MMC driver for Linux

## Description

The driver is a character device `/dev/virtual_mmc_driver` that emulates a 1 MB virtual MMC card. Data storage is implemented in kernel RAM, so the device preserves written data for as long as the module remains loaded.

The standard `read()` and `write()` system calls are not supported; interaction with the device is performed exclusively through the `ioctl` interface.

Only the following MMC commands are supported:

- `MMC_READ_SINGLE_BLOCK`    17
- `MMC_READ_MULTIPLE_BLOCK`  18
- `MMC_WRITE_BLOCK`          24
- `MMC_WRITE_MULTIPLE_BLOCK` 25

MMC command handling is implemented through the `mmc_ioc_cmd` structure defined in `<linux/mmc/ioctl.h>`.

All operations are performed in 512-byte blocks. A maximum of 2048 blocks is supported.

The driver validates request parameters, including:
- address alignment
- allowed block count
- access beyond virtual memory boundaries

The driver distinguishes between `ioctl` system call errors and errors returned by the virtual MMC card.

For card errors, standard status flags are returned:
- `R1_ADDRESS_ERROR`
- `R1_BLOCK_LEN_ERROR`
- `R1_OUT_OF_RANGE`
- `R1_ILLEGAL_COMMAND`

## Example usage

### 1. Loading the module into the kernel

```bash
sudo ./ci/load.sh
```

### 2. Running the utility for interacting with the driver

- `--op` — MMC command code: `17`, `18`, `24`, or `25`
- `--offset` — byte offset, must be a multiple of `512`
- `--count` — number of `512`-byte blocks
- `--input` — file with data for write commands
- `--output` — file for data read from the device

For write operations, `--input` must point to an existing file. For read operations, `--output` sets the file where the result will be written; the file may already exist or be created.

```bash
cd user && make
# for write operations
./user_program --op <operation_code> --offset <offset_bytes> --count <block_count> --input <file>

# for read operations
./user_program --op <operation_code> --offset <offset_bytes> --count <block_count> --output <file>
```

Example:

```bash
./user_program --op 18 --offset 0 --count 10 --output test.bin
```

> [!NOTE]
> The card contains 2048 blocks. `--count` must not exceed `2048 - offset / 512`.

### 3. Unloading the module

```bash
./ci/unload.sh
```

## Benchmark

Load the module and run:

```bash
sudo ./ci/benchmark.sh
```

## Tests

The tests check data persistence and correct handling of card errors.

Load the module and run:

```bash
sudo ./ci/test.sh
```

# Драйвер виртуальной мультимедиа карты

## Описание

Драйвер представляет собой символьное устройство `/dev/virtual_mmc_driver`, эмулирующее виртуальную MMC-карту объёмом 1 МБ. Хранение данных реализовано в оперативной памяти ядра, благодаря чему устройство сохраняет записанные данные на протяжении всего времени, пока модуль загружен.

Стандартные системные вызовы `read()` и `write()` не поддерживаются — взаимодействие с устройством осуществляется исключительно через интерфейс `ioctl`.

Поддерживаются только данные MMC-команды:

- `MMC_READ_SINGLE_BLOCK`    17
- `MMC_READ_MULTIPLE_BLOCK`  18
- `MMC_WRITE_BLOCK`          24
- `MMC_WRITE_MULTIPLE_BLOCK` 25

Обработка MMC-команд реализована через структуру `mmc_ioc_cmd`, определённую в `<linux/mmc/ioctl.h>`.

Все операции выполняются блоками по 512 байт. Максимально поддерживается 2048 блоков.

Драйвер проверяет корректность параметров запросов, включая:
- выравнивание адресов
- допустимое количество блоков
- выход за границы виртуальной памяти

Драйвер различает ошибки системного вызова `ioctl` и ошибки, возвращаемые виртуальной MMC-картой.

В случае ошибок карты возвращаются стандартные флаги статуса:
- `R1_ADDRESS_ERROR`
- `R1_BLOCK_LEN_ERROR`
- `R1_OUT_OF_RANGE`
- `R1_ILLEGAL_COMMAND`

## Пример использования

### 1. Загрузка модуля в ядро

```bash
sudo ./ci/load.sh
```

### 2. Запуск утилиты для взаимодействия с драйвером

- `--op` — код MMC-команды: `17`, `18`, `24` или `25`
- `--offset` — смещение в байтах, должно быть кратно `512`
- `--count` — количество блоков по `512` байт
- `--input` — файл с данными для команд записи
- `--output` — файл для результата команд чтения

Для записи `--input` должен указывать на существующий файл. Для чтения `--output` задаёт файл, в который будет записан результат; файл может уже существовать или быть создан заново.

```bash
cd user && make
#for write operations
./user_program --op <operation_code> --offset <offset_bytes> --count <block_count> --input <file>

#for read operations
./user_program --op <operation_code> --offset <offset_bytes> --count <block_count> --output <file>
```
Например
```bash
./user_program --op 18 --offset 0 --count 10 --output test.bin
```


> [!NOTE]
> Доступный диапазон карты — 2048 блоков. Значение `--count` не должно превышать `2048 - offset / 512`.



### 3. Выгрузка модуля

```bash
./ci/unload.sh
```

## Бенчмарк

Загрузите модуль и запустите:

```bash
sudo ./ci/benchmark.sh
```

## Тесты

Тесты проверяют персистентность данных и корректность обработки ошибок карты.

Загрузите модуль и запустите:

```bash
sudo ./ci/test.sh
```
