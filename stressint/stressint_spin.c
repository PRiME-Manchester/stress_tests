/*
cc  stressint.c cpuidc.c -lrt -lc -lm -O3 -o stressInt
*/

#include "spin1_api.h"

#define TIMER_TICK_PERIOD 1000
#define BUFFER_SIZE 32
/*
  Use integer values only!

  K or KBytes               Integer number of bytes to use
  S or Seconds              Running time for each of 12 tests
  L or Log                  Log file name extension 0 to 99
*/
#define MEMKB    8
#define SECS     1
#define LOG      0
#define PASSES   1

// From cpuidh.h and cpuidc.c
uint   t1;
float  millisecs, secs;

// --------------------------
uint *xx, **xx_sdram;
uint RrunSecs;
uint useMemKB;
uint testlog;

uint pattern1[16], pattern2[16];
uint usePattern1,  usePattern2;
uint isdata[16],  sbdata[16];
uint errword[16], sumCheck[8];
uint test_DMA = 0;

int TrunSecs;
int testPasses = PASSES;
int errorTest;
int errors = 0;
int errord, errorp;

float maxMIPS0, maxMIPS1, maxMIPS2;
float results[30][10];

//  ************** PROTOPTYPES *****************
void checkData(uint mempasses);
void writeData(uint mempasses);
void errorcheck(uint mempasses);
void readData(uint mempasses, uint outerLoop, uint passWords);
void CRC32_poly(void);
void write_read_test(void);
void read_test(void);

void app_init(void);
void app_done(void);
void count_packets (uint key, uint payload);
void do_transfer (uint val, uint none);

int intg(float num);
int frac(float num, uint precision);

// Timer prototypes
void start_time();
void end_time();


// *************** MAIN ****************
int c_main(int argc, char *argv[])
{         
  RrunSecs = SECS;
  useMemKB = MEMKB;
  testlog  = LOG;

  // set timer tick value (in microseconds)
  spin1_set_timer_tick (TIMER_TICK_PERIOD);

  // register callbacks
  //spin1_callback_on (MCPL_PACKET_RECEIVED, count_packets, -1);
  //spin1_callback_on (DMA_TRANSFER_DONE, check_memcopy, 0);
  //spin1_callback_on (USER_EVENT, send_packets, 2);
  //spin1_callback_on (TIMER_TICK, flip_led, 3);

  // initialize application
  app_init ();

  // Write/Read Test (1st test)
  write_read_test();

  // Read Test (2nd test)
  // read_test();

  // go 
//  spin1_start(SYNC_WAIT);

  // report results
  app_done();

  return 0;
}

void app_done()
{
  //sark_xfree(sv->sdram_heap, xx, ALLOC_LOCK);
  sark_free(xx);
}

void app_init(void)
{
  // -----------------------------------------------
  // Configure spinnaker Timer2
  // En Mode Int X Pre Size One-shot (Prescaler 256)
  // 1  0    0   X 10  1    0        = 0x8a
  tc[T2_CONTROL] = 0x0000008a;
  // -----------------------------------------------     

  if (RrunSecs > 10)
  {
    float tp = (float)RrunSecs / 10.0;
    testPasses = (int)tp;
    RrunSecs = (int)((float)RrunSecs / (float)testPasses);
  }
  TrunSecs = RrunSecs * testPasses;

  pattern1[0]  = 0x00000000;
  pattern1[1]  = 0xFFFFFFFF;
  pattern1[2]  = 0xA5A5A5A5;
  pattern1[3]  = 0x55555555;
  pattern1[4]  = 0x33333333;
  pattern1[5]  = 0xF0F0F0F0;
  pattern1[6]  = 0x00000000;
  pattern1[7]  = 0xFFFFFFFF;
  pattern1[8]  = 0xA5A5A5A5;
  pattern1[9]  = 0x55555555;
  pattern1[10] = 0x33333333;
  pattern1[11] = 0xF0F0F0F0;

  pattern2[0]  = 0x00000000;
  pattern2[1]  = 0xFFFFFFFF;
  pattern2[2]  = 0x5A5A5A5A;
  pattern2[3]  = 0xAAAAAAAA;
  pattern2[4]  = 0xCCCCCCCC;
  pattern2[5]  = 0x0F0F0F0F;
  pattern2[6]  = 0xFFFFFFFF;
  pattern2[7]  = 0x00000000;
  pattern2[8]  = 0x5A5A5A5A;
  pattern2[9]  = 0xAAAAAAAA;
  pattern2[10] = 0xCCCCCCCC;
  pattern2[11] = 0x0F0F0F0F;

  // -------------------------------
  io_printf(IO_BUF, " ------------------------------------------------------------\n");
  io_printf(IO_BUF, "  Integer Stress Test\n");
  io_printf(IO_BUF, "  %d KBytes Cache or RAM Space, %d Seconds Per Test, 12 Tests\n\n", useMemKB, TrunSecs);

  // Allocate SDRAM
  //xx = (uint *)sark_xalloc (sv->sdram_heap, sizeof(uint)*useMemKB*1024+256, 0, ALLOC_LOCK);
  
  // Allocate TCM memory
  //xx = (uint *)sark_alloc (useMemKB*1024+256, sizeof(uint)); // why 1024?? 256 should be enough
  xx = (uint *)sark_alloc (useMemKB*256, sizeof(uint));
  if (xx  == NULL)
    io_printf(IO_BUF, " Unable to allocate TCM memory.\n");

  // Allocate SDRAM memory
  xx_sdram = (uint *)sark_xalloc (sv->sdram_heap, useMemKB*256, 0, ALLOC_LOCK);
  if (xx_sdram  == NULL)
    io_printf(IO_BUF, " Unable to allocate SDRAM memory.\n");


  io_printf(IO_BUF, " Write/Read\n");
  errorTest = 0;
  maxMIPS1  = 0;
  maxMIPS2  = 0;

  // Configure CRC32 polynomial
  CRC32_poly();

}


void write_read_test(void)
{
  int j, n1, p;
  uint mempasses;
  uint tempPattern, printPattern;
  float n1Count, timeCount;
  float millionBytes, mb_s;

  mempasses = useMemKB * 8;   // 128 bytes or 32 words per pass

  // ################  Part 1   
  for (j=6; j<12; j++)
  {
    printPattern = pattern1[j];
    usePattern1  = pattern2[j];
    usePattern2  = pattern1[j];
    millionBytes = (float) useMemKB * 1.024 / 1000.0;

    results[j][0] = 0;
    n1Count = 0;
    timeCount = 0;
   
    for (p=0; p<testPasses; p++)
    {
      n1 = 0;
      start_time();
      do
      {
        tempPattern = usePattern1;
        usePattern1 = usePattern2;
        usePattern2 = tempPattern;
        xx[0]   = usePattern1;
        xx[1]   = usePattern1;
        xx[2]   = usePattern1;
        xx[3]   = usePattern1;
        xx[4]   = usePattern2;
        xx[5]   = usePattern2;
        xx[6]   = usePattern2;
        xx[7]   = usePattern2;
        errorTest = errorTest + 1;
        writeData(mempasses);

        // Test for error reports one word
        //        if (errorTest == 1 && p == 0 && j == 6) xx[1] = 99;                                
        checkData(mempasses);
        n1 = n1 + 1;
        end_time();
      }
      while (secs < (float)RrunSecs && errors == 0);
      n1Count = n1Count + (float)n1;
      timeCount = timeCount + secs;

      mb_s = millionBytes * 2 * (float)n1 / secs;
      io_printf(IO_BUF, " Log%2d Pass %3d Test%2d of 6, Pattern %08x, %2d.%01d Secs, %3d.%02d MB/S\n",
                 testlog, p+1, j-5, printPattern, intg(secs), frac(secs,1), intg(mb_s), frac(mb_s,2));
    }
    results[j][0] = millionBytes * 2 * n1Count / timeCount;
    maxMIPS0 = n1Count * 140 * (float)mempasses / 1000000 / timeCount; // write 37 check 103 
    if (maxMIPS0 > maxMIPS1) maxMIPS1 = maxMIPS0;
  }
  io_printf(IO_BUF, "                    Maximum speed %2d.%02d MIPS\n\n", intg(maxMIPS1), frac(maxMIPS1,2));
}



void read_test(void)
{
  int j, n1, p;
  uint mempasses, passWords;
  uint printPattern;
  uint outerLoop, outerLoopc;

  float n1Count, timeCount;
  float millionBytes, mb_s;

  // ################  Part 2
  for (j=0; j<6; j++) // 0 to 6
  {
    printPattern = pattern1[j];
    usePattern1 = pattern1[j];   
    usePattern2 = pattern2[j];   
    
    millionBytes = (float) useMemKB * 1.024 / 1000;

    xx[0]   = usePattern1;
    xx[1]   = usePattern1;
    xx[2]   = usePattern1;
    xx[3]   = usePattern1;
    xx[4]   = usePattern2;
    xx[5]   = usePattern2;
    xx[6]   = usePattern2;
    xx[7]   = usePattern2;

    mempasses = useMemKB * 8;
    writeData(mempasses);
    checkData(mempasses);

    results[j][0] = 0;

    passWords = 16;
    mempasses = useMemKB * 256 / passWords;

    outerLoop = 1;
    start_time();
    readData(mempasses, outerLoop, passWords);
    end_time();

    outerLoop = 100;
    if (secs > 0.001) outerLoop = (uint)(0.1/secs);
    if (outerLoop < 1) outerLoop = 1;
    n1Count = 0;
    timeCount = 0;
    outerLoopc = outerLoop;
    
    for (p=0; p<testPasses; p++)
    {
      n1 = 0;
      start_time();
      do
      {
        outerLoop = outerLoopc;
        readData(mempasses, outerLoop, passWords);
        n1 = n1 + 1;
        end_time();
      }
      while (secs < (float)RrunSecs);
      n1Count = n1Count + (float)n1;
      timeCount = timeCount + secs;
      outerLoop = outerLoopc;
      
      mb_s = millionBytes * (float)outerLoop * (float)n1 / secs;
      io_printf(IO_BUF, " Log%2d Pass%3d Test%2d of 6, Pattern %08x, %3d.%01d Secs, %3d.%02d MB/S\n",
                 testlog, p+1, j+1, printPattern, intg(secs), frac(secs,1), intg(mb_s), frac(mb_s,2));
    }
    results[j][0] = millionBytes * (float)outerLoop * n1Count / timeCount;
    float icount = (38 * (float)mempasses + 11) * (float)outerLoop;
    maxMIPS0 = n1Count * icount / 1000000 / timeCount;
    if (maxMIPS0 > maxMIPS2) maxMIPS2 = maxMIPS0;
    checkData(mempasses);
  }

  io_printf(IO_BUF, "                    Maximum speed %6d.%02d MIPS\n", intg(maxMIPS2), frac(maxMIPS2, 2));
}


void CRC32_poly(void)
{
  // CRC32 polynomials
  dma[DMA_CRCT + 0]  = 0xFB808B20;
  dma[DMA_CRCT + 1]  = 0x7DC04590;
  dma[DMA_CRCT + 2]  = 0xBEE022C8;
  dma[DMA_CRCT + 3]  = 0x5F701164;
  dma[DMA_CRCT + 4]  = 0x2FB808B2;
  dma[DMA_CRCT + 5]  = 0x97DC0459;
  dma[DMA_CRCT + 6]  = 0xB06E890C;
  dma[DMA_CRCT + 7]  = 0x58374486;
  dma[DMA_CRCT + 8]  = 0xAC1BA243;
  dma[DMA_CRCT + 9]  = 0xAD8D5A01;
  dma[DMA_CRCT + 10] = 0xAD462620;
  dma[DMA_CRCT + 11] = 0x56A31310;
  dma[DMA_CRCT + 12] = 0x2B518988;
  dma[DMA_CRCT + 13] = 0x95A8C4C4;
  dma[DMA_CRCT + 14] = 0xCAD46262;
  dma[DMA_CRCT + 15] = 0x656A3131;
  dma[DMA_CRCT + 16] = 0x493593B8;
  dma[DMA_CRCT + 17] = 0x249AC9DC;
  dma[DMA_CRCT + 18] = 0x924D64EE;
  dma[DMA_CRCT + 19] = 0xC926B277;
  dma[DMA_CRCT + 20] = 0x9F13D21B;
  dma[DMA_CRCT + 21] = 0xB409622D;
  dma[DMA_CRCT + 22] = 0x21843A36;
  dma[DMA_CRCT + 23] = 0x90C21D1B;
  dma[DMA_CRCT + 24] = 0x33E185AD;
  dma[DMA_CRCT + 25] = 0x627049F6;
  dma[DMA_CRCT + 26] = 0x313824FB;
  dma[DMA_CRCT + 27] = 0xE31C995D;
  dma[DMA_CRCT + 28] = 0x8A0EC78E;
  dma[DMA_CRCT + 29] = 0xC50763C7;
  dma[DMA_CRCT + 30] = 0x19033AC3;
  dma[DMA_CRCT + 31] = 0xF7011641;
}

void writeData(uint mempasses)
{
  int i;
  uint ecx = xx[0];
  uint edx = xx[4];
  uint eax = 8;

  for (i=0; i<mempasses; i++)
  {
    xx[eax]    = ecx;
    xx[eax+1]  = ecx;
    xx[eax+2]  = ecx;
    xx[eax+3]  = ecx;

    xx[eax+4]  = edx;
    xx[eax+5]  = edx;
    xx[eax+6]  = edx;
    xx[eax+7]  = edx;

    xx[eax+8]  = ecx;
    xx[eax+9]  = ecx;
    xx[eax+10] = ecx;
    xx[eax+11] = ecx;

    xx[eax+12] = edx;
    xx[eax+13] = edx;
    xx[eax+14] = edx;
    xx[eax+15] = edx;

    xx[eax+16] = ecx;
    xx[eax+17] = ecx;
    xx[eax+18] = ecx;
    xx[eax+19] = ecx;

    xx[eax+20] = edx;
    xx[eax+21] = edx;
    xx[eax+22] = edx;
    xx[eax+23] = edx;

    xx[eax+24] = ecx;
    xx[eax+25] = ecx;
    xx[eax+26] = ecx;
    xx[eax+27] = ecx;

    xx[eax+28] = edx;
    xx[eax+29] = edx;
    xx[eax+30] = edx;
    xx[eax+31] = edx;

    // schedule DMA transfer callback
    spin1_schedule_callback (do_transfer, payload, 0, 1);

    eax += 32;
  }
}

void do_transfer(uint val, uint none)
{
    spin1_dma_transfer(DMA_WRITE, xx_sdram, xx, DMA_WRITE, BUFFER_SIZE*sizeof(uint));
}


void checkData(uint mempasses)
{
  uint i, m;

  sumCheck[0] = usePattern1;
  sumCheck[1] = usePattern2;

  mempasses = useMemKB * 8; // 1024 / 128;            
  errorcheck(mempasses);
  
  errors = 0;
  errord = 0;

  if (sumCheck[0] != usePattern1 ||
      sumCheck[1] != usePattern1 || 
      sumCheck[2] != usePattern2 ||
      sumCheck[3] != usePattern2 || errorp == 1)
  {
    for (m=0; m<useMemKB*128; m+=8) {
      if (sumCheck[0] != usePattern1 ||
          sumCheck[1] != usePattern1 || errorp == 1) {    
        for (i=0; i<4; i++) {
          if (xx[m+i] != usePattern1) {
            errors = errors + 1;
            if (errors < 13) {
                isdata[errord] = xx[m+i];
                sbdata[errord] = usePattern1;
                errword[errord] = m+i;
                errord = errord + 1;
            }
          }
        }
      }
      if (sumCheck[2] != usePattern2 ||
          sumCheck[3] != usePattern2 || errorp == 1) 
      {
        for (i=4; i<8; i++) {
          if (xx[m+i] != usePattern2) {
            errors = errors + 1;
            if (errors < 13) {
                isdata[errord] = xx[m+i];
                sbdata[errord] = usePattern2;
                errword[errord] = m+i+4;
                errord = errord + 1;
            }
          }
        }
      }
    }
  }
}

void errorcheck(uint mempasses)
{
  int i;
  uint eax = 0;
  uint ecx = sumCheck[0];
  uint edx = sumCheck[0];
  uint edi = sumCheck[1];
  uint esi = sumCheck[1];
  for (i=0; i<mempasses; i++)
  {        
       ecx = ecx | xx[eax]    |  xx[eax+1] | xx[eax+2]  | xx[eax+3];
       edi = edi | xx[eax+4]  |  xx[eax+5] | xx[eax+6]  | xx[eax+7];
       ecx = ecx | xx[eax+8]  |  xx[eax+9] | xx[eax+10] | xx[eax+11];
       edi = edi | xx[eax+12] | xx[eax+13] | xx[eax+14] | xx[eax+15];
       ecx = ecx | xx[eax+16] | xx[eax+17] | xx[eax+18] | xx[eax+19];
       edi = edi | xx[eax+20] | xx[eax+21] | xx[eax+22] | xx[eax+23];
       ecx = ecx | xx[eax+24] | xx[eax+25] | xx[eax+26] | xx[eax+27];
       edi = edi | xx[eax+28] | xx[eax+29] | xx[eax+30] | xx[eax+31];
       edx = edx & xx[eax]    &  xx[eax+1] & xx[eax+2]  & xx[eax+3];
       esi = esi & xx[eax+4]  &  xx[eax+5] & xx[eax+6]  & xx[eax+7];
       edx = edx & xx[eax+8]  &  xx[eax+9] & xx[eax+10] & xx[eax+11];
       esi = esi & xx[eax+12] & xx[eax+13] & xx[eax+14] & xx[eax+15];
       edx = edx & xx[eax+16] & xx[eax+17] & xx[eax+18] & xx[eax+19];
       esi = esi & xx[eax+20] & xx[eax+21] & xx[eax+22] & xx[eax+23];
       edx = edx & xx[eax+24] & xx[eax+25] & xx[eax+26] & xx[eax+27];
       esi = esi & xx[eax+28] & xx[eax+29] & xx[eax+30] & xx[eax+31];

       eax += 32;         
  }
  sumCheck[0] = ecx;
  sumCheck[1] = edx;
  sumCheck[2] = edi;
  sumCheck[3] = esi;   
}

void readData(uint mempasses, uint outerLoop, uint passWords)
{
  int i, j;
  uint eax;
  uint ecx = xx[0];
  uint edx = xx[0];
  uint edi = xx[0];
  uint esi = xx[0];

  for (j=0; j<outerLoop; j++)
  {
      eax = 0;        
      for (i=0; i<mempasses; i++)
      {
          ecx = ecx - xx[eax];
          edx = edx - xx[eax+1];       
          edi = edi - xx[eax+2];       
          esi = esi - xx[eax+3];

          ecx = ecx - xx[eax+4];
          edx = edx - xx[eax+5];       
          edi = edi - xx[eax+6];       
          esi = esi - xx[eax+7];

          ecx = ecx + xx[eax+8];
          edx = edx + xx[eax+9];       
          edi = edi + xx[eax+10];       
          esi = esi + xx[eax+11];

          ecx = ecx + xx[eax+12];
          edx = edx + xx[eax+13];       
          edi = edi + xx[eax+14];       
          esi = esi + xx[eax+15];

          eax = eax + passWords;                   
      }        
  }

  ecx = ecx - edx;
  ecx = ecx + edi;
  ecx = ecx - esi;
  ecx = ecx + edi;
  xx[0] = ecx;
}


/******************************/
/* Timer functions            */
/******************************/
void start_time()
{
  t1 = tc[T2_COUNT];
}

void end_time()
{
  millisecs = (t1 - tc[T2_COUNT])*1.28/1000.0;
  secs      = millisecs/1000.0;
}

// Return integer part
int intg(float num)
{
  return (int)num;
}

// Return fractional part
int frac(float num, uint precision)
{ 
  int m=1;

  if (precision>0)
    for (int i=0; i<precision; i++)
      m*=10;
      
  return (int)((num-(int)num)*m);
}

