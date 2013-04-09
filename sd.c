/* File:    sd.c
 * 
 * Author:  David Zemon
 */

// Includes
#include <sd.h>

// SPI config
#define SD_SPI_INIT_FREQ			200000					// Run SD initialization at 200 kHz
#define SD_SPI_FINAL_FREQ			1900000					// Speed clock to 1.8 MHz after initialization
#define SD_SPI_POLARITY				SPI_POLARITY_LOW		// SD cards like low polarity
#define SD_SPI_MODE_OUT				SPI_MSB_FIRST
#define SD_SPI_MODE_IN				SPI_MSB_PRE
#define SD_SPI_BYTE_IN_SZ			1

// Misc. SD Definitions
#define SD_WIGGLE_ROOM				10000
#define SD_RESPONSE_TIMEOUT			CLKFREQ/10			// Wait 0.1 seconds for a response before timing out
#define SD_SECTOR_SIZE				512

// SD Commands
#define SD_CMD_IDLE					0x40 + 0			// Send card into idle state
#define	SD_CMD_SDHC					0x40 + 8			// Set SD card version (1 or 2) and voltage level range
#define SD_CMD_RD_CSD				0x40 + 9			// Request "Card Specific Data" block contents
#define SD_CMD_RD_CID				0x40 + 10			// Request "Card Identification" block contents
#define SD_CMD_RD_BLOCK				0x40 + 17			// Request data block
#define SD_CMD_READ_OCR				0x40 + 58			// Request "Operating Conditions Register" contents
#define SD_CMD_APP					0x40 + 55			// Inform card that following instruction is application specific
#define SD_CMD_WR_OP				0x40 + 41			// Send operating conditions for SDC
// SD Arguments
#define SD_CMD_VOLT_ARG				0x000001AA
#define SD_ARG_LEN					5

// SD CRCs
#define SD_CRC_IDLE					0x95
#define SD_CRC_SDHC					0x87
#define SD_CRC_ACMD					0x77
#define SD_CRC_OTHER				0x01

// SD Responses
#define SD_RESPONSE_IDLE			0x01
#define SD_RESPONSE_ACTIVE			0x00
#define SD_DATA_START_ID			0xFE
#define SD_RESPONSE_LEN_R1			1
#define SD_RESPONSE_LEN_R3			5
#define	SD_RESPONSE_LEN_R7			5

// Boot sector addresses/values
#define SD_FAT_16					2					// A FAT entry in FAT16 is 2-bytes
#define SD_FAT_32					4					// A FAT entry in FAT32 is 4-bytes
#define SD_BOOT_SECTOR_ID			0xeb
#define SD_BOOT_SECTOR_ID_ADDR		0
#define SD_BOOT_SECTOR_BACKUP		0x1c6
#define SD_CLUSTER_SIZE_ADDR		0x0d
#define SD_RSVD_SCTR_CNT_ADDR		0x0e
#define SD_NUM_FATS_ADDR			0x10
#define SD_ROOT_ENTRY_CNT_ADDR		0x11
#define SD_TOT_SCTR_16_ADDR			0x13
#define SD_FAT_SIZE_ADDR			0x16
#define SD_TOT_SCTR_32_ADDR			0x20
#define SD_FAT_SIZE_32_ADDR			0x24
#define SD_ROOT_CLUSTER_ADDR		0x2c
#define SD_FAT12_CLSTR_CNT			4085
#define SD_FAT16_CLSTR_CNT			65525

uint32 g_cs;
uint8 g_filesystem;
uint8 g_sectorsPerCluster;
uint32 g_fatSize;
uint32 g_totalSectors, g_dataSectors, g_rootDirSectors;
uint32 g_fatStart, g_rootAddr, g_firstData;

#ifdef SD_DEBUG
uint8 g_sd_invalidResponse;
#endif

/***********************************
 *** Private Function Prototypes ***
 ***********************************/
/* @Brief: Send a command and argument over SPI to the SD card
 *
 * @param    command       6-bit value representing the command sent to the SD card
 *
 * @return
 */
uint8 SDSendCommand (const uint8 cmd, const uint32 arg, const uint8 crc);

/* Brief: Receive response and data from SD card over SPI
 *
 * @param	bytes		Number of bytes to receive
 * @param	*data		Location in memory with enough space to store 'bytes' bytes of data
 *
 * @return		Returns 0 for success, else error code
 */
uint8 SDGetResponse (const uint8 numBytes, uint8 *dat);

/* @Brief: Receive data from SD card via SPI
 *
 * @param	bytes		Number of bytes to receive
 * @param	*data		Location in memory with enough space to store 'bytes' bytes of data
 * @param	address		Address for the SD card from which data should be read
 *
 * @return		Returns 0 for success, else error code
 */
uint8 SDReadBlock (uint16 bytes, uint8 *dat);

/* @Brief: Read SD_SECTOR_SIZE-byte data block from SD card
 *
 * @param	address		Block address to read from SD card
 * @param	*dat		Location in chip memory to store data block
 *
 * @return		Returns 0 upon success, error code otherwise
 */
uint8 SDReadDataBlock (uint32 address, uint8 *dat);

// TODO: Document these prototypes
uint16 SDConvertDat16 (const uint8 dat[]);
uint32 SDConvertDat32 (const uint8 dat[]);

#ifdef SD_DEBUG
#include <stdio.h>
#include <stdarg.h>
/* Brief: Print an error through UART string followed by entering an infinite loop
 *
 * @param	err		Error number used to determine error string
 */
void SDError (const uint8 err, ...);

/* @Brief: Print to screen each status bit individually with human-readable descriptions
 *
 * @param	response		first-byte response from the SD card
 */
void SDFirstByteExpansion (const uint8 response);
#else
// Exit calling function by returning 'err'
#define SDError(err, ...)				return err
#endif

/****************************
 *** Function Definitions ***
 ****************************/
uint8 SDStart (const uint32 mosi, const uint32 miso, const uint32 sclk, const uint32 cs) {
	uint16 i, err;
	uint8 response[16];

	// Set CS for output and initialize high
	g_cs = cs;
	GPIODirModeSet(cs, GPIO_DIR_OUT);
	GPIOPinSet(cs);

	// Start SPI module
	if (err = SPIStart(mosi, miso, sclk, SD_SPI_INIT_FREQ, SD_SPI_POLARITY))
		SDError(err);

	for (i = 0; i < 10; ++i) {
		waitcnt(CLKFREQ/2 + CNT);
		// Send at least 72 clock cycles to enable the SD card
		GPIOPinSet(cs);
		for (i = 0; i < 5; ++i)
			SPIShiftOut(16, -1, SD_SPI_MODE_OUT);

		GPIOPinClear(cs);
		// Send SD into idle state, retrieve a response and ensure it is the "idle" response
		if (err = SDSendCommand(SD_CMD_IDLE, 0, SD_CRC_IDLE))
			SDError(err);
		SDGetResponse(SD_RESPONSE_LEN_R1, response);
		if (SD_RESPONSE_IDLE == response[0])
			i = 10;
	}
	if (SD_RESPONSE_IDLE != response[0])
		SDError(SD_INVALID_INIT, response[0]);

#if (defined SD_VERBOSE && defined SD_DEBUG)
	__simple_printf("Sending CMD8...\n");
#endif

	// Set voltage to 3.3V and ensure response is R7
	if (err = SDSendCommand(SD_CMD_SDHC, SD_CMD_VOLT_ARG, SD_CRC_SDHC))
		SDError(err);
	if (err = SDGetResponse(SD_RESPONSE_LEN_R7, response))
		SDError(err);
	if ((SD_RESPONSE_IDLE != response[0]) || (0x01 != response[3])
			|| (0xAA != response[4]))
		SDError(SD_INVALID_INIT, response[0]);

#if (defined SD_VERBOSE && defined SD_DEBUG)
	__simple_printf("CMD8 succeeded. Requesting operating conditions...\n");
#endif

	// Request operating conditions register and ensure response begins with R1
	if (err = SDSendCommand(SD_CMD_READ_OCR, 0, SD_CRC_OTHER))
		SDError(err);
	if (err = SDGetResponse(SD_RESPONSE_LEN_R3, response))
		SDError(err);
#if (defined SD_VERBOSE && defined SD_DEBUG)
	SDPrintHexBlock(response, SD_RESPONSE_LEN_R3);
#endif
	if (SD_RESPONSE_IDLE != response[0])
		SDError(SD_INVALID_INIT, response[0]);

	// Spin up the card and bring to active state
#if (defined SD_VERBOSE && defined SD_DEBUG)
	__simple_printf("OCR read successfully. Sending into active state...\n");
#endif
	for (i = 0; i < 8; ++i) {
		if (err = SDSendCommand(SD_CMD_APP, 0, SD_CRC_OTHER))
			SDError(err);
		if (err = SDGetResponse(1, response))
			SDError(err);
		if (err = SDSendCommand(SD_CMD_WR_OP, BIT_30, SD_CRC_OTHER))
			SDError(err);
		SDGetResponse(1, response);
		if (SD_RESPONSE_ACTIVE == response[0])
			break;
	}
	if (SD_RESPONSE_ACTIVE != response[0])
		SDError(SD_INVALID_RESPONSE, response[0]);
#if (defined SD_VERBOSE && defined SD_DEBUG)
	__simple_printf("Activated!\n");
#endif

	// Initialization nearly complete, increase clock
	SPISetClock(SD_SPI_FINAL_FREQ);

// If debugging requested, print to the screen CSD and CID registers from SD card
#if (defined SD_VERBOSE && defined SD_DEBUG)
	__simple_printf("Requesting CSD...\n");
	if (err = SDSendCommand(SD_CMD_RD_CSD, 0, SD_CRC_OTHER))
		SDError(err);
	if (err = SDReadBlock(16, response))
		SDError(err);
	__simple_printf("CSD Contents:\n");
	SDPrintHexBlock(response, 16);
	putchar('\n');

	__simple_printf("Requesting CID...\n");
	if (err = SDSendCommand(SD_CMD_RD_CID, 0, SD_CRC_OTHER))
		SDError(err);
	if (err = SDReadBlock(16, response))
		SDError(err);
	__simple_printf("CID Contents:\n");
	SDPrintHexBlock(response, 16);
	putchar('\n');
#endif
	GPIOPinSet(cs);

	// Initialization complete
	return 0;
}

uint8 SDMount (void) {
	uint8 buf[SD_SECTOR_SIZE];
	uint8 err;

	// FAT system determination variables:
	uint32 sectorsPerCluster, rsvdSectorCount, numFATs, rootEntryCount, totalSectors,
			FATSize, rootCluster;
	uint32 bootSector = 0;
	uint32 clusterCount;

	// Read in first sector
	if (err = SDReadDataBlock(bootSector, buf))
		SDError(err);
	// Check if sector 0 is boot sector or MBR; if MBR, skip to boot sector
	if (SD_BOOT_SECTOR_ID != buf[SD_BOOT_SECTOR_ID_ADDR]) {
		bootSector = SDConvertDat32(&(buf[SD_BOOT_SECTOR_BACKUP]));
		if (err = SDReadDataBlock(bootSector, buf))
			SDError(err);
	}

	// Print the boot sector if requested
#if (defined SD_VERBOSE && defined SD_DEBUG)
	__simple_printf("***BOOT SECTOR***\n");
	SDPrintHexBlock(buf, SD_SECTOR_SIZE);
	putchar('\n');
#endif

	// Do this whether it is FAT16 or FAT32
	sectorsPerCluster = buf[SD_CLUSTER_SIZE_ADDR];
	rsvdSectorCount = SDConvertDat16(&buf[SD_RSVD_SCTR_CNT_ADDR]);
	numFATs = buf[SD_NUM_FATS_ADDR];
	rootEntryCount = SDConvertDat16(&buf[SD_ROOT_ENTRY_CNT_ADDR]);

	// Check if FAT size is valid in 16- or 32-bit location
	FATSize = SDConvertDat16(&buf[SD_FAT_SIZE_ADDR]);
	if (!FATSize)
		FATSize = buf[SD_FAT_SIZE_32_ADDR];

	// Check if FAT16 total sectors is valid
	totalSectors = SDConvertDat16(&buf[SD_TOT_SCTR_16_ADDR]);
	if (!totalSectors)
		totalSectors = SDConvertDat32(&buf[SD_TOT_SCTR_32_ADDR]);

	// Compute necessary numbers to determine FAT type (12/16/32)
	g_rootDirSectors = (rootEntryCount * 32 + SD_SECTOR_SIZE - 1) / SD_SECTOR_SIZE;
	g_dataSectors = totalSectors - (rsvdSectorCount + numFATs * FATSize + rootEntryCount);
	clusterCount = g_dataSectors / sectorsPerCluster;

#if (defined SD_DEBUG && defined SD_VERBOSE)
	printf("FAT Size: 0x%04X / %u\n", FATSize, FATSize);
	printf("Root directory sectors: 0x%08X / %u\n", g_rootDirSectors, g_rootDirSectors);
	printf("Total sector count: 0x%08X / %u\n", totalSectors, totalSectors);
	printf("Cluster count: 0x%08X / %u\n", clusterCount, clusterCount);
	printf("Data sectors: 0x%08X / %u\n", g_dataSectors, g_dataSectors);
	printf("Reserved sector count: 0x%08X / %u\n", rsvdSectorCount, rsvdSectorCount);
	printf("Root entry count: 0x%08X / %u\n", rootEntryCount, rootEntryCount);
#endif

	// Determine and store FAT type
	if (SD_FAT12_CLSTR_CNT > clusterCount)
		SDError(SD_INVALID_FILESYSTEM);
	else if (SD_FAT16_CLSTR_CNT > clusterCount) {
#if (defined SD_VERBOSE && defined SD_DEBUG)
		__simple_printf("FAT type is FAT16\n");
#endif
		g_filesystem = SD_FAT_16;
	} else {
#if (defined SD_VERBOSE && defined SD_DEBUG)
		__simple_printf("FAT type is FAT32\n");
#endif
		g_filesystem = SD_FAT_32;
	}

	// Find start of FAT
	g_fatStart = bootSector + rsvdSectorCount;

//	g_filesystem = SD_FAT_16;
	// Find root directory address
	switch (g_filesystem) {
		case SD_FAT_16:
			g_rootAddr = FATSize * numFATs + g_fatStart;
			g_firstData = g_rootAddr + rootEntryCount;
			break;
		case SD_FAT_32:
			rootCluster = SDConvertDat32(&buf[SD_ROOT_CLUSTER_ADDR]);
			g_rootAddr = g_fatStart + rootCluster;
			g_firstData = g_rootAddr;
			break;
	}

	// Prints stuffffsssss
#if (defined SD_VERBOSE && defined SD_DEBUG)
	printf("Start of FAT: 0x%08X\n", g_fatStart);
	printf("Root directory: 0x%08X\n", g_rootAddr);
	printf("First data sector: 0x%08X\n", g_firstData);
#endif

	if (err = SDReadDataBlock(g_rootAddr, buf))
		SDError(err);

#if (defined SD_VERBOSE && defined SD_DEBUG)
	SDPrintHexBlock(buf, SD_SECTOR_SIZE);
#endif

	return 0;
}

uint8 SDSendCommand (const uint8 cmd, const uint32 arg, const uint8 crc) {
	uint8 err;

// Send out the command
	if (err = SPIShiftOut(8, cmd, SD_SPI_MODE_OUT))
		return err;

// Send argument
	if (err = SPIShiftOut(16, (arg >> 16), SD_SPI_MODE_OUT))
		return err;
	if (err = SPIShiftOut(16, arg & WORD_0, SD_SPI_MODE_OUT))
		return err;

// Send sixth byte - CRC
	if (err = SPIShiftOut(8, crc, SD_SPI_MODE_OUT))
		return err;

	return 0;
}

uint8 SDGetResponse (uint8 bytes, uint8 *dat) {
	uint8 err;
	uint32 timeout;

// Read first byte - the R1 response
	timeout = SD_RESPONSE_TIMEOUT + CNT;
	do {
		if (err = SPIShiftIn(8, SD_SPI_MODE_IN, dat, SD_SPI_BYTE_IN_SZ))
			return err;

		// Check for timeout
		if (0 < (timeout - CNT) && (timeout - CNT) < SD_WIGGLE_ROOM)
			return SD_READ_TIMEOUT;
	} while (0xff == *dat); // wait for transmission end

	if ((SD_RESPONSE_IDLE == *dat) || (SD_RESPONSE_ACTIVE == *dat)) {
		++dat;		// Increment pointer to next byte;
		--bytes;	// Decrement bytes counter

		// Read remaining bytes
		while (bytes--)
			if (err = SPIShiftIn(8, SD_SPI_MODE_IN, dat++, SD_SPI_BYTE_IN_SZ))
				return err;
	} else {
#ifdef SD_DEBUG
		g_sd_invalidResponse = *dat;
#endif
		return SD_INVALID_RESPONSE;
	}

	SPIShiftOut(8, 0xff, SD_SPI_MODE_OUT);

	return 0;
}

uint8 SDReadBlock (uint16 bytes, uint8 *dat) {
	uint8 i, err, checksum;
	uint32 timeout;

	if (!bytes)
		SDError(SD_INVALID_NUM_BYTES);

// Read first byte - the R1 response
	timeout = SD_RESPONSE_TIMEOUT + CNT;
	do {
		if (err = SPIShiftIn(8, SD_SPI_MODE_IN, dat, SD_SPI_BYTE_IN_SZ))
			return err;

		// Check for timeout
		if (0 < (timeout - CNT) && (timeout - CNT) < SD_WIGGLE_ROOM)
			return SD_READ_TIMEOUT;
	} while (0xff == *dat); // wait for transmission end

// Ensure this response is "active"
	if (SD_RESPONSE_ACTIVE == *dat) {
		//++dat;			// Commented out to write over this byte later (uncomment for debugging purposes only)

		// Ignore blank data again
		timeout = SD_RESPONSE_TIMEOUT + CNT;
		do {
			if (err = SPIShiftIn(8, SD_SPI_MODE_IN, dat, SD_SPI_BYTE_IN_SZ))
				return err;

			// Check for timeout
			if ((timeout - CNT) < SD_WIGGLE_ROOM)
				return SD_READ_TIMEOUT;
		} while (0xff == *dat); // wait for transmission end

		// Check for the data start identifier and continue reading data
		if (SD_DATA_START_ID == *dat) {
			//++dat;			// Commented out to write over this byte later (uncomment for debugging purposes only)
			// Read in requested data bytes
			while (bytes--) {
#ifdef SD_DEBUG
				if (err = SPIShiftIn(8, SD_SPI_MODE_IN, dat++, SD_SPI_BYTE_IN_SZ))
					return err;
#else
				SPIShiftIn(8, SD_SPI_MODE_IN, dat++, SD_SPI_BYTE_IN_SZ);
#endif
			}

			// Read two more bytes for checksum - throw away data
			for (i = 0; i < 2; ++i) {
				timeout = SD_RESPONSE_TIMEOUT + CNT;
				do {
					if (err = SPIShiftIn(8, SD_SPI_MODE_IN, &checksum, SD_SPI_BYTE_IN_SZ))
						return err;

					// Check for timeout
					if ((timeout - CNT) < SD_WIGGLE_ROOM)
						return SD_READ_TIMEOUT;
				} while (0xff == checksum); // wait for transmission end
			}

			// Send final 0xff
			if (err = SPIShiftOut(8, 0xff, SD_SPI_MODE_OUT))
				return err;
		} else {
#ifdef SD_DEBUG
			g_sd_invalidResponse = *dat;
#endif
			return SD_INVALID_DAT_STRT_ID;
		}
	} else {
#ifdef SD_DEBUG
		g_sd_invalidResponse = *dat;
#endif
		return SD_INVALID_RESPONSE;
	}

	return 0;
}

uint8 SDReadDataBlock (uint32 address, uint8 *dat) {
	uint8 err;

	GPIOPinClear(g_cs);
	if (err = SDSendCommand(SD_CMD_RD_BLOCK, address, SD_CRC_OTHER))
		SDError(err);

	if (err = SDReadBlock(SD_SECTOR_SIZE, dat))
		SDError(err);
	GPIOPinSet(g_cs);

	return 0;
}

uint16 SDConvertDat16 (const uint8 dat[]) {
	return (dat[1] << 8) + dat[0];
}

uint32 SDConvertDat32 (const uint8 dat[]) {
	return (dat[3] << 24) + (dat[2] << 16) + (dat[1] << 8) + dat[0];
}

#ifdef SD_VERBOSE
uint8 SDPrintHexBlock (uint8 *dat, uint16 bytes) {
	uint8 i, j;
	uint8 *s;

	__simple_printf("Printing %u bytes...\n", bytes);
	__simple_printf("Offset\t");
	for (i = 0; i < SD_LINE_SIZE; ++i)
		printf("0x%X  ", i);
	putchar('\n');

	if (bytes % SD_LINE_SIZE)
		bytes = bytes / SD_LINE_SIZE + 1;
	else
		bytes /= SD_LINE_SIZE;

	for (i = 0; i < bytes; ++i) {
		s = (uint8 *) (dat + SD_LINE_SIZE * i);
		printf("0x%04X:\t", SD_LINE_SIZE * i);
		for (j = 0; j < SD_LINE_SIZE; ++j)
			printf("0x%02X ", s[j]);
		__simple_printf(" - ");
		for (j = 0; j < SD_LINE_SIZE; ++j) {
			if ((' ' <= s[j]) && (s[j] <= '~'))
				putchar(s[j]);
			else
				putchar('.');
		}

		putchar('\n');
	}

	return 0;
}
#endif

#ifdef SD_DEBUG
void SDError (const uint8 err, ...) {
	va_list list;
	char str[] = "SD Error %u: %s\n";

	switch (err) {
		case SD_INVALID_CMD:
			__simple_printf(str, (err - SD_ERRORS_BASE), "Invalid command");
			break;
		case SD_READ_TIMEOUT:
			__simple_printf(str, (err - SD_ERRORS_BASE), "Timed out during read");
			break;
		case SD_INVALID_NUM_BYTES:
			__simple_printf(str, (err - SD_ERRORS_BASE), "Invalid number of bytes");
			break;
		case SD_INVALID_RESPONSE:
#ifdef SD_VERBOSE
			printf("SD Error %u: %s0x%02X\nThe following bits are set:\n",
					(err - SD_ERRORS_BASE), "Invalid first-byte response\n\tReceived: ",
					g_sd_invalidResponse);
#else
			__simple_printf("SD Error %u: %s%u\n", (err - SD_ERRORS_BASE),
					"Invalid first-byte response\n\tReceived: ", g_sd_invalidResponse);
#endif
			SDFirstByteExpansion(g_sd_invalidResponse);
			break;
		case SD_INVALID_DAT_STRT_ID:
#ifdef SD_VERBOSE
			printf("SD Error %u: %s0x%02X\n", (err - SD_ERRORS_BASE),
					"Invalid data-start ID\n\tReceived: ", g_sd_invalidResponse);
#else
			__simple_printf("SD Error %u: %s%u\n", (err - SD_ERRORS_BASE),
					"Invalid data-start ID\n\tReceived: ", g_sd_invalidResponse);
#endif
			break;
		case SD_INVALID_INIT:
#ifdef SD_VERBOSE
			printf("SD Error %u: %s\n\tResponse: 0x%02X\n", (err - SD_ERRORS_BASE),
					"Invalid response during initialization", va_arg(list, uint32));
#else
			va_start(list, 1);
			__simple_printf("SD Error %u: %s\n\tResponse: %u\n", (err - SD_ERRORS_BASE),
					"Invalid response during initialization", va_arg(list, uint32));
			va_end(list);
#endif
			break;
		default:
			// Is the error an SPI error?
			if (err > SD_ERRORS_BASE && err < (SD_ERRORS_BASE + SD_ERRORS_LIMIT))
				__simple_printf("Unknown SD error %u\n", (err - SD_ERRORS_BASE));
			// If not, print unknown error
			else
				__simple_printf("Unknown error %u\n", err);
			break;
	}
	while (1)
		;
}

void SDFirstByteExpansion (const uint8 response) {
	if (BIT_0 & response)
		__simple_printf("\t0: Idle\n");
	if (BIT_1 & response)
		__simple_printf("\t1: Erase reset\n");
	if (BIT_2 & response)
		__simple_printf("\t2: Illegal command\n");
	if (BIT_3 & response)
		__simple_printf("\t3: Communication CRC error\n");
	if (BIT_4 & response)
		__simple_printf("\t4: Erase sequence error\n");
	if (BIT_5 & response)
		__simple_printf("\t5: Address error\n");
	if (BIT_6 & response)
		__simple_printf("\t6: Parameter error\n");
	if (BIT_7 & response)
		__simple_printf(
				"\t7: Something is really screwed up. This should always be 0.\n");
}
#endif
