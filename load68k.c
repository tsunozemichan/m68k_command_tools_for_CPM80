#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include </arch/z80.h>

// SORD M68固有のハードウェアポート定義
#define BANK_REG        0xDF    // バンクレジスタポート
#define PAGE_SWITCH     0xD2    // ページ切替ポート
#define M68K_RESET      0xD5    // m68kリセット/起動ポート

// デフォルト設定
#define DEFAULT_LOAD_ADDR   0x010000    // デフォルトのロードアドレス
#define DEFAULT_SSP         0x00FF00    // デフォルトのSSP値
#define BUFFER_SIZE         1024        // ファイル読み込みバッファサイズ

// 関数プロトタイプ宣言
int strcmp(const char *s1, const char *s2);
unsigned char read_m68k_byte(unsigned long addr);
void write_m68k_byte(unsigned long addr, unsigned char value);
void reset_m68k(void);
void start_m68k(void);
void set_m68k_vectors(unsigned long ssp, unsigned long pc);
unsigned long hex_to_long(const char *hex_str);
int load_file_to_memory(const char *filename, unsigned long load_addr);

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

// ファイルをメモリにロードする関数
int load_file_to_memory(const char *filename, unsigned long load_addr) {
    FILE *fp;
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    unsigned long addr = load_addr;
    unsigned long total_bytes = 0;
    
    // ファイルを開く
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Error: Could not open file '%s'\n", filename);
        return -1;
    }
    
    printf("Loading file to address 0x%08lX...\n", load_addr);
    
    // ファイルを読み込んでメモリに書き込む
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        printf("Writing %ld bytes at address 0x%08lX\n", (long)bytes_read, addr);
        
        // 各バイトをメモリに書き込む
        for (size_t i = 0; i < bytes_read; i++) {
            write_m68k_byte(addr + i, buffer[i]);
            
            // 進捗表示（1024バイト毎にドットを表示）
            if ((i % 1024) == 0) {
                printf(".");
                fflush(stdout);
            }
        }
        
        // アドレスとカウンタを更新
        addr += bytes_read;
        total_bytes += bytes_read;
        printf("\n");
    }
    
    // ファイルを閉じる
    fclose(fp);
    
    printf("\nLoaded %ld bytes to memory.\n", total_bytes);
    
    // ベリファイチェック（オプション）
    printf("\nVerifying...\n");
    
    // ファイルを再度開く
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Error: Could not reopen file for verification.\n");
        return -1;
    }
    
    // ファイルを再度読み込んでメモリと比較
    addr = load_addr;
    int errors = 0;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            unsigned char mem_byte = read_m68k_byte(addr + i);
            if (mem_byte != buffer[i]) {
                printf("Verification error at 0x%08lX: File=0x%02X, Memory=0x%02X\n",
                       addr + i, buffer[i], mem_byte);
                errors++;
                if (errors >= 10) {
                    printf("Too many errors, aborting verification.\n");
                    fclose(fp);
                    return -1;
                }
            }
            
            // 進捗表示（4096バイト毎にドットを表示）
            if ((i % 4096) == 0) {
                printf(".");
                fflush(stdout);
            }
        }
        addr += bytes_read;
    }
    
    // ファイルを閉じる
    fclose(fp);
    
    if (errors == 0) {
        printf("\nVerification successful!\n");
        return 0;
    } else {
        printf("\nVerification failed with %d errors.\n", errors);
        return -1;
    }
}

int main(int argc, char *argv[]) {
    unsigned long load_addr = DEFAULT_LOAD_ADDR;  // デフォルトのロードアドレス
    unsigned long ssp = DEFAULT_SSP;              // デフォルトのSSP値
    int run_after_load = 0;                       // ロード後に実行するフラグ
    int verify_load = 1;                          // ロード後に検証するフラグ
    const char *filename = NULL;                  // 初期化
    
    printf("SORD M68 MC68000 Binary Loader v1.0\n");
    printf("----------------------------------\n\n");
    
    // 引数チェック
    if (argc < 2) {
        printf("Usage: load68k <filename> [<load_address>] [-run] [-noverify]\n");
        printf("   ex: load68k program.bin\n");
        printf("       load68k program.bin $10000\n");
        printf("       load68k program.bin 0x20000 -run\n");
        printf("Options:\n");
        printf("   <load_address>  Address to load the file (default: 0x%08lX)\n", DEFAULT_LOAD_ADDR);
        printf("   -run            Run MC68000 after loading (PC=load_address)\n");
        printf("   -noverify       Skip verification after loading\n");
        return 1;
    }
    
    // デバッグ情報: コマンドライン引数の表示
    printf("Command line arguments (%d):\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  Arg %d: '%s'\n", i, argv[i]);
    }
    printf("\n");
    
    // 最初の引数は常にファイル名
    filename = argv[1];
    
    // 2番目の引数がアドレスかオプションかを判断
    if (argc >= 3) {
        if (argv[2][0] == '-') {
            // オプション引数
            if (strcasecmp(argv[2], "-run") == 0) {
                run_after_load = 1;
            } else if (strcasecmp(argv[2], "-noverify") == 0) {
                verify_load = 0;
            }
        } else {
            // ロードアドレス
            load_addr = hex_to_long(argv[2]);
        }
    }
    
    // 3番目の引数がオプションかを判断（あれば）
    if (argc >= 4) {
        if (strcasecmp(argv[3], "-run") == 0) {
            run_after_load = 1;
        } else if (strcasecmp(argv[3], "-noverify") == 0) {
            verify_load = 0;
        }
    }
    
    // 4番目の引数がオプションかを判断（あれば）
    if (argc >= 5) {
        if (strcasecmp(argv[4], "-run") == 0) {
            run_after_load = 1;
        } else if (strcasecmp(argv[4], "-noverify") == 0) {
            verify_load = 0;
        }
    }
    
    printf("File: %s\n", filename);
    printf("Load address: 0x%08lX\n", load_addr);
    if (run_after_load) {
        printf("Will run MC68000 after loading (PC=0x%08lX)\n", load_addr);
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
    
    // ファイルをメモリにロード
    if (load_file_to_memory(filename, load_addr) != 0) {
        printf("Failed to load file. Aborting.\n");
        return 1;
    }
    
    // プログラムを実行する場合
    if (run_after_load) {
        printf("\nStarting MC68000 program...\n");
        
        // m68kをリセット
        reset_m68k();
        
        // SSPとPCの設定
        set_m68k_vectors(ssp, load_addr);
        
        // m68kを起動
        start_m68k();
        
        printf("MC68000 program started.\n");
        printf("SSP: 0x%08lX\n", ssp);
        printf("PC:  0x%08lX\n", load_addr);
    }
    
    printf("\nOperation completed successfully.\n");
    return 0;
}