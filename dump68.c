#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include </arch/z80.h>

// SORD M68固有のハードウェアポート定義
#define BANK_REG        0xDF    // バンクレジスタポート
#define PAGE_SWITCH     0xD2    // ページ切替ポート
#define M68K_RESET      0xD5    // m68kリセット/起動ポート

// ダンプ表示の定数
#define BYTES_PER_LINE  16      // 1行に表示するバイト数
#define MAX_LINES       23      // 1ページに表示する最大行数

// 関数プロトタイプ宣言
int strcmp(const char *s1, const char *s2);
unsigned char read_m68k_byte(unsigned long addr);
void write_m68k_byte(unsigned long addr, unsigned char value);
void reset_m68k(void);
void start_m68k(void);
void set_m68k_vectors(unsigned long ssp, unsigned long pc);
unsigned long hex_to_long(const char *hex_str);
int is_printable(unsigned char c);
void dump_memory(unsigned long start_addr, unsigned long end_addr);

// グローバル変数 - アドレスと値の受け渡し用
unsigned long g_addr = 0;       // アクセスするアドレス
unsigned char g_value = 0;      // 読み書きする値
unsigned char g_bank = 0;       // バンク値（アドレスの上位8ビット）
unsigned short g_offset = 0;    // オフセット値（アドレスの下位16ビット）

// m68kメモリから1バイト読み込む関数
unsigned char read_m68k_byte(unsigned long addr) {
    // アドレスをグローバル変数に設定
    g_addr = addr;
    g_bank = (unsigned char)((addr >> 16) & 0xFF);
    g_offset = (unsigned short)(addr & 0xFFFF);
    
    // バンクレジスタの設定
    z80_outp(BANK_REG, g_bank);
    
    // インラインアセンブラ - オフセットをHLに設定
    #asm
    ; オフセットをHLに設定
    ld hl,(_g_offset)
    
    ; 割り込み禁止
    di
    
    ; ページ切替とメモリ読み取り
    out (0xD2),a   ; EXフラグをセット（次の1命令のみページ1）
    ld a,(hl)      ; ページ1から読み込み
    
    ; 割り込み許可
    ei
    
    ; 値をグローバル変数に格納
    ld (_g_value),a
    #endasm
    
    return g_value;
}

// m68kメモリに1バイト書き込む関数
void write_m68k_byte(unsigned long addr, unsigned char value) {
    // アドレスと値をグローバル変数に設定
    g_addr = addr;
    g_bank = (unsigned char)((addr >> 16) & 0xFF);
    g_offset = (unsigned short)(addr & 0xFFFF);
    g_value = value;
    
    // バンクレジスタの設定
    z80_outp(BANK_REG, g_bank);
    
    // インラインアセンブラ - オフセットをHLに設定、値をAに設定
    #asm
    ; オフセットをHLに設定
    ld hl,(_g_offset)
    
    ; 書き込む値をAに設定
    ld a,(_g_value)
    
    ; 割り込み禁止
    di
    
    ; ページ切替とメモリ書き込み
    out (0xD2),a   ; EXフラグをセット（次の1命令のみページ1）
    ld (hl),a      ; ページ1に書き込み
    
    ; 割り込み許可
    ei
    #endasm
}

// m68kをリセットする関数
void reset_m68k() {
    z80_outp(M68K_RESET, 0x00);
}

// m68kを起動する関数
void start_m68k() {
    z80_outp(M68K_RESET, 0x01);
}

// SSPとPCを設定する関数
void set_m68k_vectors(unsigned long ssp, unsigned long pc) {
    // SSPの設定（アドレス0-3）
    write_m68k_byte(0x000000, (unsigned char)((ssp >> 24) & 0xFF));
    write_m68k_byte(0x000001, (unsigned char)((ssp >> 16) & 0xFF));
    write_m68k_byte(0x000002, (unsigned char)((ssp >> 8) & 0xFF));
    write_m68k_byte(0x000003, (unsigned char)(ssp & 0xFF));
    
    // PCの設定（アドレス4-7）
    write_m68k_byte(0x000004, (unsigned char)((pc >> 24) & 0xFF));
    write_m68k_byte(0x000005, (unsigned char)((pc >> 16) & 0xFF));
    write_m68k_byte(0x000006, (unsigned char)((pc >> 8) & 0xFF));
    write_m68k_byte(0x000007, (unsigned char)(pc & 0xFF));
}

// 16進数表示用関数
void print_hex_byte(unsigned char value) {
    printf("%02X", value);
}

// 16進数文字列を数値に変換
unsigned long hex_to_long(const char *hex_str) {
    unsigned long value = 0;
    
    // 先頭の "0x" または "$" をスキップ
    if (hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
        hex_str += 2;
    } else if (hex_str[0] == '$') {
        hex_str += 1;
    }
    
    // 16進数文字列を数値に変換
    while (*hex_str) {
        char c = *hex_str++;
        value <<= 4;
        
        if (c >= '0' && c <= '9') {
            value += c - '0';
        } else if (c >= 'A' && c <= 'F') {
            value += c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            value += c - 'a' + 10;
        } else {
            // 不正な文字は無視
            break;
        }
    }
    
    return value;
}

// 文字として表示可能かチェック
int is_printable(unsigned char c) {
    return (c >= 32 && c <= 126);
}

// メモリをダンプする関数
void dump_memory(unsigned long start_addr, unsigned long end_addr) {
    unsigned long addr;
    int i, count = 0;
    unsigned char byte;
    
    // 16バイト単位でアドレスを丸める
    start_addr = start_addr & 0xFFFFFFF0;
    
    // 各行16バイト表示
    for (addr = start_addr; addr <= end_addr; addr += BYTES_PER_LINE) {
        // アドレス表示
        printf("%08lX: ", addr);
        
        // 16バイト分の16進数表示
        for (i = 0; i < BYTES_PER_LINE; i++) {
            if (addr + i <= end_addr) {
                byte = read_m68k_byte(addr + i);
                print_hex_byte(byte);
            } else {
                printf("  "); // 範囲外は空白表示
            }
            
            // 8バイト毎に余分なスペース
            if (i == 7) {
                printf(" ");
            }
            printf(" ");
        }
        
        // ASCII文字表示部分
        printf("| ");
        for (i = 0; i < BYTES_PER_LINE; i++) {
            if (addr + i <= end_addr) {
                byte = read_m68k_byte(addr + i);
                if (is_printable(byte)) {
                    printf("%c", byte);
                } else {
                    printf("."); // 表示できない文字はドットで表示
                }
            } else {
                printf(" "); // 範囲外は空白表示
            }
        }
        printf(" |");
        printf("\n");
        
        // 大量のデータを表示する場合、ページ単位で一時停止
        // count++;
        // if (count % MAX_LINES == 0 && addr + BYTES_PER_LINE <= end_addr) {
        //     printf("-- Press Enter to continue --");
        //     getchar();
        // }
    }
}

int main(int argc, char *argv[]) {
    unsigned long start_addr, end_addr;
    unsigned long ssp = 0x00FF00; // デフォルトのSSP値
    int init_m68k = 0;            // m68kを初期化するフラグ
    
    printf("SORD M68 MC68000 Memory Dump Utility v2.0\n");
    printf("---------------------------------------\n\n");
    
    // 引数チェック
    if (argc < 2) {
        printf("Usage: dump68 <start_address> [<end_address>] [-init]\n");
        printf("   ex: dump68 $10000 $1003F\n");
        printf("       dump68 0x10000 0x1003F\n");
        printf("       dump68 $10000 $1003F -init\n");
        printf("Options:\n");
        printf("   -init   Initialize MC68000 (SSP=$00FF00, PC=start_address)\n");
        return 1;
    }
    
    // 開始アドレスを取得
    start_addr = hex_to_long(argv[1]);
    
    // 終了アドレスを取得（指定がなければ開始アドレスから256バイト）
    if (argc >= 3 && argv[2][0] != '-') {
        end_addr = hex_to_long(argv[2]);
        
        // -initオプションチェック
        if (argc >= 4 && strcmp(argv[3], "-init") == 0) {
            init_m68k = 1;
        }
    } else {
        end_addr = start_addr + 255;
        
        // -initオプションチェック
        if (argc >= 3 && strcmp(argv[2], "-init") == 0) {
            init_m68k = 1;
        }
    }
    
    // 範囲チェック
    if (start_addr > end_addr) {
        printf("Error: Start address must be less than or equal to end address.\n");
        return 1;
    }
    
    // 表示サイズの制限（必要に応じて調整）
    if (end_addr - start_addr > 65535) {
        printf("Warning: Display limited to 64KB from start address.\n");
        end_addr = start_addr + 65535;
    }
    
    printf("Initialization check...\n");
    // 簡単なメモリ動作テスト
    write_m68k_byte(0x000100, 0xA5);
    if (read_m68k_byte(0x000100) == 0xA5) {
        printf("Memory R/W test: OK\n\n");
    } else {
        printf("Memory R/W test: FAILED\n");
        printf("Basic memory access not working. Aborting.\n");
        return 1;
    }
    
    // m68kの初期化
    if (init_m68k) {
        printf("Initializing MC68000...\n");
        // m68kをリセット
        reset_m68k();
        
        // SSPとPCの設定
        set_m68k_vectors(ssp, start_addr);
        
        // m68kを起動
        // start_m68k();
        
        printf("MC68000 initialized and started.\n");
        printf("SSP set to 0x%08lX\n", ssp);
        printf("PC set to 0x%08lX\n\n", start_addr);
    }
    
    printf("Dumping memory from %08lX to %08lX\n\n", start_addr, end_addr);
    
    // メモリダンプを実行
    dump_memory(start_addr, end_addr);
    
    printf("\nDump complete.\n");
    return 0;
}