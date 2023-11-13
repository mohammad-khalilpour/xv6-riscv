//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "histBuff.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x
#define MAX_HISTORY 16
#define INPUT_BUF_SIZE 128

struct historyBufferArray histBuff;

//
// send one character to the uart.
// called by printf(), and to echo input characters,
// but not from write().
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

struct {
  struct spinlock lock;
  
  // input
  char buf[INPUT_BUF_SIZE];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} cons;

//
// user write()s to the console go here.
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;

  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    uartputc(c);
  }

  return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while(cons.r == cons.w){
      if(killed(myproc())){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);
    }

    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    if(c == C('D')){  // end-of-file
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // a whole line has arrived, return to
      // the user-level read().
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}

//
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
uint64 commandSize;
void saveCommandToHistory();

void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch(c){
  case C('P'):  // Print process list.
    procdump();
    break;
  case C('U'):  // Kill line.
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // Backspace
  case '\x7f': // Delete key
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
      commandSize --;
    }
    break;
  default:
    if(c == '\x41'){
      histBuff.arrowKeyIndex++;
      while (cons.e > 0) {
          consputc(BACKSPACE);
          cons.e--;
      }
      cons.r = 0;
      int temp = (histBuff.lastCommandIndex - histBuff.arrowKeyIndex) % histBuff.numOfCommandsInMem;
      if (temp < 0)
          temp += histBuff.numOfCommandsInMem;
      commandSize = 0;
      for (int i = 0; i < histBuff.lengthArr[temp]; i++) {
          consputc(histBuff.bufferArr[temp][i]);
          cons.buf[cons.e++ % INPUT_BUF_SIZE] = histBuff.bufferArr[temp][i];
      }
      commandSize = histBuff.lengthArr[temp];
    }
    else if (c == '\x42') {
      histBuff.arrowKeyIndex--;
        while (cons.e > 0) {
            consputc(BACKSPACE);
            cons.e--;
        }
      cons.r = 0;
      int temp = (histBuff.lastCommandIndex - histBuff.arrowKeyIndex) % histBuff.numOfCommandsInMem;
      commandSize = 0;
      for (int i = 0; i < histBuff.lengthArr[temp]; i++) {
          consputc(histBuff.bufferArr[temp][i]);
          cons.buf[cons.e++ % INPUT_BUF_SIZE] = histBuff.bufferArr[temp][i];
      }
    }
    else if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.
      consputc(c);

      // store for consumption by consoleread().
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;
      commandSize++;

      if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
        // wake up consoleread() if a whole line (or end-of-file)
        // has arrived.
        saveCommandToHistory();
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);
}

void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}

void
saveCommandToHistory() {
    commandSize--;
    int isHistoryCommand = 1;
    char hist[8] = {'h','i','s','t','o','r','y','\0'};
    for(int i=0;i<7;i++) {
        if (cons.buf[cons.r + i] != hist[i]) {
            isHistoryCommand = 0;
            break;
        }
    }

    if (!isHistoryCommand) {
        for (int i = 0; i < commandSize; i++) {
            histBuff.bufferArr[histBuff.lastCommandIndex][i] = cons.buf[cons.r + i];
        }
        histBuff.lengthArr[histBuff.lastCommandIndex] = commandSize;
        histBuff.lastCommandIndex++;
        histBuff.numOfCommandsInMem++;
        if (histBuff.lastCommandIndex == 16)
            histBuff.lastCommandIndex = 0;
        if(histBuff.numOfCommandsInMem > 16)
            histBuff.numOfCommandsInMem = 16;
    }
    commandSize = 0;
}