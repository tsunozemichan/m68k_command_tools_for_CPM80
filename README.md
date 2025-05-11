# SORD M68のCP/M-80上で内蔵のmc68000を動かすためのツール

## 使用したコンパイラ

コンパイルはz88dkを使用。


## Dump68.com

### 機能

指定したメモリアドレス範囲を表示する

### 使い方

"-init"オプションをつけると、m68kを初期化(停止)する。SORD M68は起動時にはm68kは停止している。

```
    printf("Usage: dump68 <start_address> [<end_address>] [-init]\n");
        printf("   ex: dump68 $10000 $1003F\n");
        printf("       dump68 0x10000 0x1003F\n");
        printf("       dump68 $10000 $1003F -init\n");
        printf("Options:\n");
        printf("   -init   Initialize MC68000 (SSP=$00FF00, PC=start_address)\n");
```

## load68k.com

### 機能

指定したアドレスに指定したファイルの内容(一般的には、m68kのバイナリファイル)を書き込む。"-run"オプションがあれば書き込み後に実行する。
"-noverify"オプションがあれば、書き込み後に検証しない。

```
    printf("Usage: load68k <filename> [<load_address>] [-run] [-noverify]\n");
        printf("   ex: load68k program.bin\n");
        printf("       load68k program.bin $10000\n");
        printf("       load68k program.bin 0x20000 -run\n");
        printf("Options:\n");
        printf("   <load_address>  Address to load the file (default: 0x%08lX)\n", DEFAULT_LOAD_ADDR);
        printf("   -run            Run MC68000 after loading (PC=load_address)\n");
        printf("   -noverify       Skip verification after loading\n");
```


# Tools to Operate the Built-in MC68000 on SORD M68 under CP/M-80

## Compiler Used

Compiled using **z88dk**.

## Dump68.com

### Features

Displays the contents of a specified memory address range.

### Usage

Add the `-init` option to initialize (halt) the MC68000.  
On startup, the MC68000 in the SORD M68 is in a halted state.

```
printf("Usage: dump68 <start_address> [<end_address>] [-init]\n");
printf("   ex: dump68 $10000 $1003F\n");
printf("       dump68 0x10000 0x1003F\n");
printf("       dump68 $10000 $1003F -init\n");
printf("Options:\n");
printf("   -init   Initialize MC68000 (SSP=$00FF00, PC=start_address)\n");
```

## load68k.com

### Features

Writes the contents of a specified file (typically a binary for the MC68000) to a specified address.  
With the `-run` option, it will execute the code after writing.  
With the `-noverify` option, it skips verification after writing.

```
printf("Usage: load68k <filename> [<load_address>] [-run] [-noverify]\n");
printf("   ex: load68k program.bin\n");
printf("       load68k program.bin $10000\n");
printf("       load68k program.bin 0x20000 -run\n");
printf("Options:\n");
printf("   <load_address>  Address to load the file (default: 0x%08lX)\n", DEFAULT_LOAD_ADDR);
printf("   -run            Run MC68000 after loading (PC=load_address)\n");
printf("   -noverify       Skip verification after loading\n");
```
