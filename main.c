#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MEM_SIZE (256 * 512) // 256 4 byte chunks (instructions) per 1 kibibyte * 256 kibibytes
                               // chunk 256 * 512 (top chunk) = program counter 4096
                               // chunk 255 * 512 (one chunk down) = program counter 4100 and so on

uint64_t tinkerRegs[32] = {0}; // array of signed 64-bit integers representing register values (should data type be int64?)
uint32_t memArray[MEM_SIZE]; // array of 32 bit integers to hold each instruction index 0 = programCounter 4096, index n = (programCounter - 4096) / 4
int memAddressCounter = 0; // counts total number of instructions for first pass through file
int isUserMode = 1; // tracks user mode
int isSupervisorMode = 0; // tracks supervisor mode

int64_t extendLiteral(uint16_t literal) {
    if (literal & 0x800) {
        return (int64_t)(literal | 0xFFFFFFFFFFFFF000ULL);
    }
    else {
        return (int64_t)literal;
    }
}

void and(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r2] & tinkerRegs[r3];
    *programCounter += 4;
    return;
}

void or(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r2] | tinkerRegs[r3];
    *programCounter += 4;
    return;
}

void xor(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r2] ^ tinkerRegs[r3];
    *programCounter += 4;
    return;
}

void not(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = ~tinkerRegs[r2];
    *programCounter += 4;
    return;
}

void shftr(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r2] >> tinkerRegs[r3];
    *programCounter += 4;
    return;
}

void shftri(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r1] >> literal;
    *programCounter += 4;
    return;
}

void shftl(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r2] << tinkerRegs[r3];
    *programCounter += 4;
    return;
}

void shftli(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r1] << literal;
    *programCounter += 4;
    return;
}

void br(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    *programCounter = tinkerRegs[r1];
    return;
}

void brr1(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
   *programCounter += tinkerRegs[r1];
    return;
}

void brr2(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    int64_t newLiteral = extendLiteral(literal);
    *programCounter += newLiteral;
    return;
}

void brnz(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    if (tinkerRegs[r2] != 0) {
        *programCounter = tinkerRegs[r1];
    }
    else {
        *programCounter += 4;
    }
    return;
}

void call(int r1, int r2, int r3, int literal, uint64_t *programCounter){
    if (tinkerRegs[31] - 2 < 4096) {
        fprintf(stderr, "Simulation error");
        exit(-1);
    }
    *(uint64_t *)(&memArray[tinkerRegs[31] - 2]) = *programCounter + 4;
    *programCounter = tinkerRegs[r1];
    return;
}

void tinkerReturn(int r1, int r2, int r3, int literal, uint64_t *programCounter){
    if (tinkerRegs[31] - 2 < 4096) {
        fprintf(stderr, "Simulation error");
        exit(-1);
    }
    *programCounter = *(uint64_t *)(&memArray[tinkerRegs[31] - 2]);
}

void brgt(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    if (tinkerRegs[r2] <= tinkerRegs[r3]) {
        *programCounter += 4;
    }
    else {
        *programCounter = tinkerRegs[r1];
    }
    return;
}

void priv(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    char inputBuffer[64];
    uint64_t input;
    if (literal == 0) {
        exit(0);
    }
    if (literal == 1) {
        isSupervisorMode = 1;
        isUserMode = 0;
    }
    else if (literal == 2) {
        isUserMode = 1;
        isSupervisorMode = 0;
    }
    else if (literal == 3) {
        if (tinkerRegs[r2] == 0) {
            if (fgets(inputBuffer, sizeof(inputBuffer), stdin) == NULL) {
                fprintf(stderr, "Simulation error");
                exit(-1);
            } 
            if (sscanf(inputBuffer, "%llu", &input) != 1) {
                fprintf(stderr, "Simulation error");
                exit(-1);
            }
            tinkerRegs[r1] = input;
        }
    }
    else if (literal == 4) {
        if (tinkerRegs[r1] != 0) {
            printf("%llu", tinkerRegs[r2]);
        }
    }
    else {
        fprintf(stderr, "Simulation error");
        exit(-1);
    }
    *programCounter += 4;
    return;
}

void mov1(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    int64_t address = tinkerRegs[r2] + extendLiteral(literal);
    if (address < 0 || address >= MEM_SIZE) { 
        fprintf(stderr, "Simulation error");
        exit(-1);
    }
    tinkerRegs[r1] = ((uint64_t)memArray[address/4 + 1] << 32) | memArray[address/4];
    *programCounter += 4;
    return;
}

void mov2(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r2]; 
    *programCounter += 4;
    return;
}

void mov3(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = (tinkerRegs[r1] & ~(0xFFFULL << 52)) | ((uint64_t)literal << 52);
    *programCounter += 4;
    return;
}

void mov4(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    int64_t address = tinkerRegs[r1] + extendLiteral(literal);
    if (address < 0 || address >= MEM_SIZE) { 
        fprintf(stderr, "Simulation error");
        exit(-1);
    }
    memArray[address/4] = (uint32_t)tinkerRegs[r2];
    memArray[address/4 + 1] = (uint32_t)(tinkerRegs[r2] >> 32);
    *programCounter += 4;
    return;
}

void addf(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    double in1, in2, result;
    memcpy(&in1, &tinkerRegs[r2], sizeof(double));
    memcpy(&in2, &tinkerRegs[r3], sizeof(double));
    result = in1 + in2;
    memcpy(&tinkerRegs[r1], &result, sizeof(double)); 
    *programCounter += 4;
    return;
}

void subf(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    double in1, in2, result;
    memcpy(&in1, &tinkerRegs[r2], sizeof(double));
    memcpy(&in2, &tinkerRegs[r3], sizeof(double));
    result = in1 - in2;
    memcpy(&tinkerRegs[r1], &result, sizeof(double)); 
    *programCounter += 4;
    return;
}

void mulf(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    double in1, in2, result;
    memcpy(&in1, &tinkerRegs[r2], sizeof(double));
    memcpy(&in2, &tinkerRegs[r3], sizeof(double));
    result = in1 * in2;
    memcpy(&tinkerRegs[r1], &result, sizeof(double)); 
    *programCounter += 4;
    return;
}

void divf(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    double in1, in2, result;
    memcpy(&in1, &tinkerRegs[r2], sizeof(double));
    memcpy(&in2, &tinkerRegs[r3], sizeof(double));
    if (in2 == 0.0) {
        fprintf(stderr, "Simulation error");
        exit(-1);
    }
    result = in1 / in2;
    memcpy(&tinkerRegs[r1], &result, sizeof(double)); 
    *programCounter += 4;
    return;
}

void add(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r2] + tinkerRegs[r3];
    *programCounter += 4;
    return;
}

void addi(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] += literal;
    *programCounter += 4;
    return;
}

void sub(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = tinkerRegs[r2] - tinkerRegs[r3];
    *programCounter += 4;
    return;
}

void subi(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] -= literal;
    *programCounter += 4;
    return;
}

void mul(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = (int64_t)(tinkerRegs[r2] * tinkerRegs[r3]);
    *programCounter += 4;
    return;
}

void tinkerDiv(int r1, int r2, int r3, int literal, uint64_t *programCounter) {
    tinkerRegs[r1] = (int64_t)(tinkerRegs[r2] / tinkerRegs[r3]);
    *programCounter += 4;
    return;
}

typedef void (*Instruction)(int r1, int r2, int r3, int literal, uint64_t *);
Instruction globalInstructionArray[30] = {and, or, xor, not, shftr, shftri, 
                                        shftl, shftli, br, brr1, brr2, brnz, 
                                        call, tinkerReturn, brgt, priv, mov1, mov2, 
                                        mov3, mov4, addf, subf, mulf, divf, add,
                                        addi, sub, subi, mul, tinkerDiv}; // array of function pointers to be called when parsing

// builds initial memory array from file
void buildFromFile(const char* fileName, uint32_t memArray[]) {
    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
            fprintf(stderr, "Error Opening Binary File");
            return;
    }
    unsigned char fourByteBuffer[4] = {0};
    while (fread(fourByteBuffer, 1, 4, file)) {
        uint32_t bigEndianVal = (fourByteBuffer[3] << 24) |
                                (fourByteBuffer[2] << 16) | 
                                (fourByteBuffer[1] << 8) | 
                                (fourByteBuffer[0]);
        memArray[memAddressCounter] = bigEndianVal;
        memAddressCounter++;
        if (memAddressCounter >= MEM_SIZE) {
            fprintf(stderr, "Simulation error");
            return;
        } 
    }
    tinkerRegs[31] = (int64_t)(MEM_SIZE - 1); // initialize stack pointer to last (top) index
    fclose(file);
}

void parseBigEndian(uint32_t instruction, int *opcode, int *r1, int *r2, int *r3, int *literal) {
    *opcode = 0x1F & (instruction >> 27);
    *r1 = 0x1F & (instruction >> 22);
    *r2 = 0x1F & (instruction >> 17);
    *r3 = 0x1F & (instruction >> 12); 
    *literal = 0xFFF & instruction; 
    return;
}

void parseFromStack(uint32_t memArray[]) {
    int opcode, r1, r2, r3, literal;
    int reachedHalt = 0;
    uint64_t programCounter = 4096;
    while (!reachedHalt) {
        if (programCounter >= MEM_SIZE) {
            fprintf(stderr, "Simulation error");
            exit(-1);
        }
        parseBigEndian(&memArray[programCounter], &opcode, &r1, &r2, &r3, &literal);
        if (opcode == 15 && literal == 0) {
            reachedHalt = 1;
            exit(0); 
        }
        globalInstructionArray[opcode](r1, r2, r3, literal, &programCounter); 
    }
    return;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Invalid tinker filepath");
        return EXIT_FAILURE;
    }
    memset(memArray, 0, sizeof(memArray));
    buildFromFile(argv[1], memArray);
    if (memAddressCounter == 0) {
        fprintf(stderr, "Simulation error");
        return EXIT_FAILURE;
    }
    parseFromStack(memArray);
    return EXIT_SUCCESS;
} 
