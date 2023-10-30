#define MAX_HISTORY 16
#define INPUT_BUF_SIZE 128


struct historyBufferArray{
    char bufferArr[MAX_HISTORY][INPUT_BUF_SIZE];
    char currentCommand[INPUT_BUF_SIZE];
    uint lengthArr[MAX_HISTORY];
    uint lastCommandIndex;
    int numOfCommandsInMem;
};

extern struct historyBufferArray histBuff;