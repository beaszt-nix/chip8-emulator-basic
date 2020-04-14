#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define STACK_LEN 32

typedef uint8_t byte;
typedef uint16_t word;

typedef struct { 
	word *arr;
	size_t stack_len;
	int top;
} stack;
void push(stack *a,word x){
	if (a->top != a->stack_len)
		a->arr[++(a->top)] = x;
}
void pop(stack *a){
	if (a->top != -1)
		a->top--;
}
word top(stack *a){
	if (a->top != -1)
		return a->arr[a->top];
}
void stack_init(stack *a){
	a->arr = malloc(STACK_LEN * sizeof(word));
	a->top = -1;
	a->stack_len = STACK_LEN;
}

byte Memory[4096];
byte Register_V[16];

word AddrI;
word PC;

stack memStack;

byte display[32][64];
byte keyState[16];
byte delayTimer;
byte soundTimer;
byte drawFlag=0;

byte chip8_fonts[80]={
  0xF0, 0x90, 0x90, 0x90, 0xF0,
  0x20, 0x60, 0x20, 0x20, 0x70,
  0xF0, 0x10, 0xF0, 0x80, 0xF0,
  0xF0, 0x10, 0xF0, 0x10, 0xF0,
  0x90, 0x90, 0xF0, 0x10, 0x10,
  0xF0, 0x80, 0xF0, 0x10, 0xF0,
  0xF0, 0x80, 0xF0, 0x90, 0xF0,
  0xF0, 0x10, 0x20, 0x40, 0x40,
  0xF0, 0x90, 0xF0, 0x90, 0xF0,
  0xF0, 0x90, 0xF0, 0x10, 0xF0,
  0xF0, 0x90, 0xF0, 0x90, 0x90,
  0xE0, 0x90, 0xE0, 0x90, 0xE0,
  0xF0, 0x80, 0x80, 0x80, 0xF0,
  0xE0, 0x90, 0x90, 0x90, 0xE0,
  0xF0, 0x80, 0xF0, 0x80, 0xF0,
  0xF0, 0x80, 0xF0, 0x80, 0x80	
};

void draw(byte x, byte y, byte n){
	byte pt_x = Register_V[x];
	byte pt_y = Register_V[y];
	byte height = n;
	byte pixel;
	Register_V[0xf] = 0;
	
	for (int yline = 0; yline < height; yline++){
		pixel = Memory[AddrI + yline];
		for (int xline = 0; xline < 8; xline++){
			if ((pixel & (0x80 >> xline))){
				if (display[y+yline][x+xline])
					Register_V[0xf] = 1;
				display[y+yline][x+xline] ^= 1;
			}
		}
	}
	drawFlag=1;
}

void BCD(byte x){
	int val = Register_V[x];
	int hun= val/100;
	int ten= (val/10) % 10;
	int one= val % 10;

	Memory[AddrI] = hun;
	Memory[AddrI + 1] = ten;
	Memory[AddrI + 2] = one;
}

byte getKey() {
	int a = getch();
	byte ret;
	switch(a){
		case '1':
			ret = 0;
			break;
		case '2':
			ret = 1;
			break;
		case '3':
			ret = 2;
			break;
		case '4':
			ret = 3;
			break;
		case 'q':
			ret = 4;
			break;
		case 'w':
			ret = 5;
			break;
		case 'e':
			ret = 6;
			break;
		case 'r':
			ret = 7;
			break;
		case 'a':
			ret = 8;
			break;
		case 's':
			ret = 9;
			break;
		case 'd':
			ret = 10;
			break;
		case 'f':
			ret = 11;
			break;
		case 'z':
			ret = 12;
			break;
		case 'x':
			ret = 13;
			break;
		case 'c':
			ret = 14;
			break;
		case 'v':
			ret = 15;
			break;
		
	}
	return ret;
}

void emu_return(){
	PC = top(&memStack);
	pop(&memStack);
}
void jmp(word op){
	PC = op;
}
void call_subroutine(word op){
	push(&memStack, PC);
	PC = op;
}
void skip_if_equal(byte x, byte nn){
	if (Register_V[x] == nn)
		PC += 2;
}
void skip_if_nequal(byte x, byte nn){
	if (Register_V[x] != nn)
		PC += 2;
}
void skip_if_reg_equal(byte x, byte y){
	if (Register_V[x] == Register_V[y])
		PC += 2;
}
void set_reg_const(byte x, byte nn){
	Register_V[x] = nn;
} 
void add_reg_const(byte x, byte nn) {
	Register_V[x] += nn;
}
void assign_reg_val(byte x, byte y){
	Register_V[x] = Register_V[y];
}
void reg_OR(byte x, byte y){
	Register_V[x] |= Register_V[y];
}
void reg_AND(byte x, byte y){
	Register_V[x] &= Register_V[y];
}
void reg_XOR(byte x, byte y){
	Register_V[x] ^= Register_V[y];
}
void reg_ADD(byte x, byte y){
	Register_V[x] += Register_V[y];
}
void reg_SUB(byte x, byte y){
	Register_V[x] -= Register_V[y];
}
void reg_NSUB(byte x, byte y){
	Register_V[x] = Register_V[y] - Register_V[x];
}
void reg_LSHIFT(byte x){
	Register_V[0xf] = Register_V[x] >> 7;
	Register_V[x] <<= 1;
}
void reg_RSHIFT(byte x){
	Register_V[0xf] = Register_V[x] & 0x1;
	Register_V[x] >>= 1;
}
void skip_if_reg_nequal(byte x, byte y){
	if (Register_V[x] != Register_V[y])
		PC += 2;
}
void set_I(word nnn){
	AddrI = nnn;
}
void jmp_add(word nnn){
	PC += Register_V[0] + nnn;
}
void set_rand_and(byte x, byte nn){
	Register_V[x] = rand() & nn;
}
void skip_if_key_press(byte x){
	if (keyState[Register_V[x]] == 1)
		PC += 2;
}
void skip_if_key_npress(byte x){
	if (keyState[Register_V[x]] == 0)
		PC += 2;
}
void set_reg_delay(byte x){
	Register_V[x] = delayTimer;
}
void set_reg_key(byte x){
	byte key = getKey();
	if (key == -1)
		PC -= 2;
	else
		Register_V[x] = key;
}
void set_delay_timer(byte x){
	delayTimer = Register_V[x];
}
void set_sound_timer(byte x){
	soundTimer = Register_V[x];
}
void add_to_I(byte x){
	AddrI += Register_V[x];
}
void load_sprite_addr_I(byte x){
	AddrI = Register_V[x] * 5;
}
void reg_dump(byte x){
	for(int i = 0; i <= x; i++){
		Memory[AddrI + i] = Register_V[i];
	}
	AddrI += x + 1;	
}
void reg_load(byte x){
	for (int i = 9; i <= x; i++){
		Register_V[i] = Memory[AddrI + i];
	}
	AddrI += x + 1;
}

void CPUReset(){
	AddrI = 0;
	PC = 0x200;
	memset(Register_V, 0, 16);
	memset(keyState, 0, 16);
	memset(display, 0, 64*32);
	memcpy(Memory,  chip8_fonts, 80);
	delayTimer = 0;
	soundTimer = 0;	
}
void loadFile(char *load_file){
	if (load_file != NULL){
		FILE *in;
		in = fopen(load_file, "rb");
		fread( &(Memory[0x200]), 0xfff, 1, in);
		fclose(in);
	}
}
word nextOp(){
	word res = 0;
	res = Memory[PC];
	res <<= 8;
	res |= Memory[PC+1];
	PC += 2;
	return res;
}
void keyPressed(int x){
	keyState[x] = 1;
}
void keyReleased(int x){
	keyState[x] = 0;
}
void decTimer(){
	if (soundTimer > 0)
		soundTimer--;
	if (delayTimer > 0)
		delayTimer--;
	if (soundTimer == 0)
		beep();
}
void disp_clear(){
	memset(&display, 0, 64*32);
	drawFlag=1;
	return;
}

void decode(word op){
	switch(op & 0xf000)
	{
		case 0x0000:
			if (op == 0x00e0)
				disp_clear();
			else if (op == 0x00ee)
				emu_return();
			break;
		case 0x1000:
			jmp(op & 0x0fff);
			break;
		case 0x2000:
			call_subroutine(op & 0x0fff);
			break;
		case 0x3000:
			skip_if_equal((0x0f00 & op) >> 8, 0x00ff & op);
			break;
		case 0x4000:
			skip_if_nequal((0x0f00 & op) >> 8, 0x00ff & op);
			break;
		case 0x5000:
			skip_if_reg_equal((0x0f00 & op) >> 8, (0x00f0 & op) >> 4);
			break;
		case 0x6000:
			set_reg_const((0x0f00 & op) >> 8, (0x00ff & op));
			break;
		case 0x7000:
			add_reg_const((0x0f000 & op) >> 8, 0x00ff & op);
			break;
		case 0x8000:
			switch( op & 0x000f ) 
			{
				case 0x0:
					assign_reg_val((0x0f00& op) >> 8, (0x00f0 & op) >> 4);
					break;
				case 0x1:
					reg_OR((0x0f00& op) >> 8, (0x00f0 & op) >> 4);			
					break;
				case 0x2:
					reg_AND((0x0f00& op) >> 8, (0x00f0 & op) >> 4);
					break;
				case 0x3:
					reg_XOR((0x0f00& op) >> 8, (0x00f0 & op) >> 4);
					break;
				case 0x4:
					reg_ADD((0x0f00& op) >> 8, (0x00f0 & op) >> 4);
					break;
				case 0x5:
					reg_SUB((0x0f00& op) >> 8, (0x00f0 & op) >> 4);
					break;
				case 0x6:
					reg_RSHIFT((0x0f00 & op) >> 8);
					break;
				case 0x7:
					reg_NSUB((0x0f00& op) >> 8, (0x00f0 & op) >> 4);
					break;
				case 0xe:
					reg_LSHIFT((0x0f00 & op) >> 8);
					break;
			}
			break;
		case 0x9000:
			skip_if_reg_nequal((0x0f00 & op) >> 8, (0x00f0 & op) >> 4);
			break;
		case 0xA000:
			set_I(0x0fff & op);
			break;
		case 0xB000:
			jmp_add(0x0fff & op);
			break;
		case 0xC000:
			set_rand_and((0x0f00 & op) >> 8, 0x00ff & op);
			break;
		case 0xD000:
			draw((0x0f00 & op) >> 8, (0x00f0 & op) >> 4, 0x000f & op);
			break;
		case 0xE000:
			if (op & 0x00ff == 0x009e)
				skip_if_key_press((0x0f00 & op) >> 8);
			else if (op & 0x00ff == 0x00a1)
				skip_if_key_npress((0x0f00 & op) >> 8);
			break;
		case 0xF000:
			switch(op & 0x00ff) 
			{
				case 0x07:
					set_reg_delay((0x0f00 & op) >> 8);
					break;
				case 0x0a:
					set_reg_key((0x0f00 & op) >> 8);
					break;
				case 0x15:
					set_delay_timer((0x0f00 & op) >> 8);
					break;
				case 0x18:
					set_sound_timer((0x0f00 & op) >> 8);
					break;
				case 0x1e:
					add_to_I((0x0f00 & op) >> 8);
					break;
				case 0x29:
					load_sprite_addr_I((0x0f00 & op) >> 8);
					break;
				case 0x33:
					BCD((0x0f00 & op) >> 8);
					break;
				case 0x55:
					reg_dump((0x0f00 & op) >> 8);
					break;
				case 0x65:
					reg_load((0x0f00 & op) >> 8);
					break;
			}
	}
}

void RenderScreen(){
	for (int y=0; y < 32; y++){
		for (int x= 0; x < 64; x++){
			move(y,x);
			if (display[y][x]){
				attron(A_REVERSE);
				printw(" ");
				attroff(A_REVERSE);
			}
			else	printw(" ");
		}
	}
}

int kbhit(){
	int ch = getch();
	if (ch != ERR){
		ungetch(ch);
		return 1;
	} else return 0;
}

void unsetKeys() {
	for (int k = 0; k < 16; k++)
		keyState[k]=0;	
}
int main(int argc, char *argv[1]){
	stack_init(&memStack);
	CPUReset();
	loadFile(argv[1]);
	initscr();
	nodelay(stdscr, TRUE);
	cbreak();
	noecho();
	while(1){
		byte key = getKey();
		if (key == 27){
			endwin();
			break;
		}
		keyPressed(key);
		word op = nextOp();
		decode(op);
		if (drawFlag)
			RenderScreen();
		refresh();
		usleep(2200);
		if (kbhit())
			unsetKeys();
	}
	getch();
	endwin();
}
