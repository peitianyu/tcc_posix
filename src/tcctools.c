/* -------------------------------------------------------------- */
/*
 *  TCC - Tiny C Compiler
 *
 *  tcctools.c - extra tools and and -m32/64 support
 *
 */

/* -------------------------------------------------------------- */
/*
 * This program is for making libtcc1.a without ar
 * tiny_libmaker - tiny elf lib maker
 * usage: tiny_libmaker [lib] files...
 * Copyright (c) 2007 Timppa
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "tcc.h"

//#define ARMAG  "!<arch>\n"
#define ARFMAG "`\n"

typedef struct {
    char ar_name[16];
    char ar_date[12];
    char ar_uid[6];
    char ar_gid[6];
    char ar_mode[8];
    char ar_size[10];
    char ar_fmag[2];
} ArHdr;

static unsigned long le2belong(unsigned long ul) {
    return ((ul & 0xFF0000)>>8)+((ul & 0xFF000000)>>24) +
        ((ul & 0xFF)<<24)+((ul & 0xFF00)<<8);
}

static int ar_usage(int ret) {
    fprintf(stderr, "usage: tcc -ar [drtvx] lib [files|@listfile]\n");
    fprintf(stderr, "  r  add/replace members, d  delete members, x  extract, t  list\n");
    fprintf(stderr, "  @listfile 逐行/空白分隔的文件列表 (规避命令行长度限制)\n");
    return ret;
}

/* 归档成员 (r 替换 / d 删除 / 新建 统一内存暂存) */
typedef struct {
    char *name;   /* 成员名 (basename, 用于同名替换/删除匹配) */
    char *data;   /* 对象文件内容 (ELF) */
    int   size;
} ArMember;

static const char *ar_basename(const char *path)
{
    const char *b = path, *s;
    for (s = path; *s; s++)
        if (*s == '/' || *s == '\\')
            b = s + 1;
    return b;
}

/* 读入现有归档的全部对象成员, 跳过符号表 "/"、"//" 长名表伪成员;
   返回 0 正常(文件不存在视为空归档), -1 归档损坏/不可读. */
static int ar_collect_existing(const char *lib, ArMember **pobjs, int *pnobjs)
{
    FILE *f;
    ArHdr h;
    char magic[8];
    char *arstr = NULL;
    int arstr_size = 0, size;
    ArMember *objs = *pobjs;
    int nobjs = *pnobjs;

    f = fopen(lib, "rb");
    if (!f)
        return 0;                       /* 归档不存在: 视为空 */
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, ARMAG, 8)) {
        fclose(f);
        return -1;
    }
    while (fread(&h, 1, sizeof h, f) == sizeof h) {
        char *p, *e, *nm, *b;
        if (memcmp(h.ar_fmag, ARFMAG, 2))
            goto bad;
        p = h.ar_name;
        for (e = p + sizeof h.ar_name; e > p && e[-1] == ' ';)
            e--;
        *e = '\0';
        h.ar_size[sizeof h.ar_size - 1] = 0;
        size = atoi(h.ar_size);
        b = tcc_malloc(size + 1);
        b[size] = 0;
        fread(b, size, 1, f);
        if (!strcmp(h.ar_name, "//")) {             /* 长名表暂存 */
            tcc_free(arstr);
            arstr = tcc_malloc(size + 1);
            memcpy(arstr, b, size);
            arstr[size] = 0;
            arstr_size = size;
        } else if (strcmp(h.ar_name, "/") && strcmp(h.ar_name, "/SYM64/")) {
            nm = h.ar_name;
            if (e > p && e[-1] == '/')
                e[-1] = 0;
            if (nm[0] == '/' && nm[1] >= '0' && nm[1] <= '9') {  /* "/<offset>" 长名 */
                char *ln, *end;
                int off = atoi(nm + 1), n;
                if (off < 0 || off >= arstr_size)
                    goto bad;
                ln = arstr + off;
                end = memchr(ln, '/', arstr_size - off);
                n = end ? (int)(end - ln) : (int)strlen(ln);
                objs = tcc_realloc(objs, (nobjs + 1) * sizeof *objs);
                objs[nobjs].name = tcc_malloc(n + 1);
                memcpy(objs[nobjs].name, ln, n);
                objs[nobjs].name[n] = 0;
                objs[nobjs].data = b;
                objs[nobjs].size = size;
                nobjs++;
            } else {
                objs = tcc_realloc(objs, (nobjs + 1) * sizeof *objs);
                objs[nobjs].name = tcc_strdup(nm);
                objs[nobjs].data = b;
                objs[nobjs].size = size;
                nobjs++;
            }
            b = NULL;                   /* 所有权转移给成员 */
        }
        if (b)
            tcc_free(b);
        if (size & 1)
            fgetc(f);
    }
    tcc_free(arstr);
    fclose(f);
    *pobjs = objs;
    *pnobjs = nobjs;
    return 0;
bad:
    tcc_free(arstr);
    fclose(f);
    return -1;
}

/* 加载一个对象文件为成员; 同名(basename)已存在则替换(保持原位), 否则追加. */
static int ar_add_member(ArMember **pobjs, int *pnobjs, const char *path, int verbose)
{
    FILE *f;
    long sz;
    char *b;
    const char *name = ar_basename(path);
    ArMember *objs;
    int i, nobjs = *pnobjs;

    f = fopen(path, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = tcc_malloc(sz + 1);
    fread(b, sz, 1, f);
    fclose(f);

    objs = *pobjs;
    for (i = 0; i < nobjs; i++)
        if (!strcmp(objs[i].name, name)) {
            tcc_free(objs[i].data);
            objs[i].data = b;
            objs[i].size = (int)sz;
            if (verbose)
                printf("r - %s\n", path);
            return 0;
        }
    objs = tcc_realloc(objs, (nobjs + 1) * sizeof *objs);
    objs[nobjs].name = tcc_strdup(name);
    objs[nobjs].data = b;
    objs[nobjs].size = (int)sz;
    nobjs++;
    *pobjs = objs;
    *pnobjs = nobjs;
    if (verbose)
        printf("a - %s\n", path);
    return 0;
}

/* 按 basename 删除成员 (同名可多个). */
static void ar_del_member(ArMember *objs, int *pnobjs, const char *name)
{
    const char *b = ar_basename(name);
    int i, n = *pnobjs;
    for (i = 0; i < n; i++) {
        if (!strcmp(objs[i].name, b)) {
            tcc_free(objs[i].data);
            tcc_free(objs[i].name);
            objs[i] = objs[n - 1];
            n--;
            i--;
        }
    }
    *pnobjs = n;
}

/* 逐 token 读取列表文件 (空白分隔, # 注释), 每个 token 交回调; <0 终止.
   供 @listfile 在 r/d 中展开输入文件/待删名字. */
static int ar_listfile_each(const char *path,
                            int (*fp)(const char *, void *), void *ctx)
{
    FILE *f;
    char line[2048];
    f = fopen(path, "rb");
    if (!f)
        return -1;
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        for (;;) {
            char *s;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
                p++;
            if (!*p || *p == '#')       /* 空行 / 注释 */
                break;
            s = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
                p++;
            *p = 0;
            if (fp(s, ctx) < 0) {
                fclose(f);
                return -1;
            }
        }
    }
    fclose(f);
    return 0;
}

struct ar_ctx_add { ArMember **objs; int *n; int verbose; };
struct ar_ctx_del { ArMember *objs; int *n; };

static int ar_add_cb(const char *tok, void *v)
{
    struct ar_ctx_add *c = v;
    return ar_add_member(c->objs, c->n, tok, c->verbose);
}
static int ar_del_cb(const char *tok, void *v)
{
    struct ar_ctx_del *c = v;
    ar_del_member(c->objs, c->n, tok);
    return 0;
}

ST_FUNC int tcc_tool_ar(int argc, char **argv)
{
    static const ArHdr arhdr_init = {
        "/               ",
        "0           ",
        "0     ",
        "0     ",
        "0       ",
        "0         ",
        ARFMAG
        };

    ArHdr arhdr = arhdr_init;
    ArHdr arhdro = arhdr_init;

    FILE *fh = NULL, *fo = NULL;
    const char *created_file = NULL; // must delete on error
    ElfW(Ehdr) *ehdr;
    ElfW(Shdr) *shdr;
    ElfW(Sym) *sym;
    int i, fsize, i_lib, i_obj;
    char *buf, *shstr, *symtab, *strtab;
    int symtabsize = 0;//, strtabsize = 0;
    char *anames = NULL;
    int *afpos = NULL;
    int istrlen, strpos = 0, fpos = 0, funccnt = 0, funcmax, hofs;
    char tfile[260], stmp[20];
    char *name;
    int ret = 2;
    const char *ops_conflict = "habiNp";  // unsupported but destructive if ignored.
    int extract = 0;
    int table = 0;
    int verbose = 0;
    /* GNU/BSD 长名表支持: "//" 伪成员保存超长文件名, 成员名写 "/<offset>" */
    int longsize = 0, sympad = 0, lnpad = 0;
    CString longnames;
    ArMember *objs = NULL;
    int nobjs = 0, del = 0, j;

    cstr_new(&longnames);
    i_lib = 0; i_obj = 0;  // will hold the index of the lib and first obj
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (*a == '-' && strchr(a, '.'))
            ret = 1; // -x.y is always invalid (same as gnu ar)
        if ((*a == '-') || (i == 1 && !strchr(a, '.'))) {  // options argument
            if (strpbrk(a, ops_conflict))
                ret = 1;
            if (strchr(a, 'x'))
                extract = 1;
            if (strchr(a, 't'))
                table = 1;
            if (strchr(a, 'v'))
                verbose = 1;
        } else {  // lib or obj files: don't abort - keep validating all args.
            if (!i_lib)  // first file is the lib
                i_lib = i;
            else if (!i_obj)  // second file is the first obj
                i_obj = i;
        }
    }

    if (!i_lib)  // i_obj implies also i_lib.
        ret = 1;
    i_obj = i_obj ? i_obj : argc;  // An empty archive will be generated if no input file is given

    if (ret == 1)
        return ar_usage(ret);

    if (extract || table) {
        if ((fh = fopen(argv[i_lib], "rb")) == NULL)
        {
            fprintf(stderr, "tcc: ar: can't open file %s\n", argv[i_lib]);
            goto finish;
        }
        fread(stmp, 1, 8, fh);
	if (memcmp(stmp,ARMAG,8))
	{
no_ar:
            fprintf(stderr, "tcc: ar: not an ar archive %s\n", argv[i_lib]);
            goto finish;
	}
	char *arstr = NULL;   /* "//" 长名表内容 */
	int arstr_size = 0;
	while (fread(&arhdr, 1, sizeof(arhdr), fh) == sizeof(arhdr)) {
	    char *p, *e;

	    if (memcmp(arhdr.ar_fmag, ARFMAG, 2))
		goto no_ar;
	    p = arhdr.ar_name;
	    for (e = p + sizeof arhdr.ar_name; e > p && e[-1] == ' ';)
		e--;
	    *e = '\0';
	    arhdr.ar_size[sizeof arhdr.ar_size-1] = 0;
	    fsize = atoi(arhdr.ar_size);
	    buf = tcc_malloc(fsize + 1);
	    fread(buf, fsize, 1, fh);
	    if (!strcmp(arhdr.ar_name, "//")) {
		/* "//" 长名表伪成员: 暂存, 供 "/<offset>" 成员名解析 */
		tcc_free(arstr);
		arstr = tcc_malloc(fsize + 1);
		memcpy(arstr, buf, fsize);
		arstr[fsize] = '\0';
		arstr_size = fsize;
	    } else if (strcmp(arhdr.ar_name,"/") && strcmp(arhdr.ar_name,"/SYM64/")) {
		char *nm = arhdr.ar_name;
		if (e > p && e[-1] == '/')
		    e[-1] = '\0';
		/* 解析 "/<offset>" 长名 (早于对象成员的 "//" 表已读过) */
		if (nm[0] == '/' && nm[1] >= '0' && nm[1] <= '9') {
		    char *ln, *end;
		    int off = atoi(nm + 1), n;
		    if (off < 0 || off >= arstr_size)
			goto no_ar;
		    ln = arstr + off;
		    end = memchr(ln, '/', arstr_size - off);
		    n = end ? (int)(end - ln) : (int)strlen(ln);
		    nm = tcc_malloc(n + 1);
		    memcpy(nm, ln, n);
		    nm[n] = '\0';
		}
		/* tv not implemented */
	        if (table || verbose)
		    printf("%s%s\n", extract ? "x - " : "", nm);
		if (extract) {
		    if ((fo = fopen(nm, "wb")) == NULL)
		    {
			fprintf(stderr, "tcc: ar: can't create file %s\n",
				nm);
			if (nm != arhdr.ar_name)
			    tcc_free(nm);
		        tcc_free(buf);
			goto finish;
		    }
		    fwrite(buf, fsize, 1, fo);
		    fclose(fo);
		    /* ignore date/uid/gid/mode */
		}
		if (nm != arhdr.ar_name)
		    tcc_free(nm);
	    }
            if (fsize & 1)
                fgetc(fh);
            tcc_free(buf);
	}
	tcc_free(arstr);
	ret = 0;
finish:
	if (fh)
		fclose(fh);
	return ret;
    }

    /* ---- 构建成员列表: @listfile + r(替换) / d(删除) ---- */
    del = (argc > 1 && strchr(argv[1], 'd')) != NULL;

    if (ar_collect_existing(argv[i_lib], &objs, &nobjs) < 0) {
        fprintf(stderr, "tcc: ar: not an ar archive %s\n", argv[i_lib]);
        goto the_end;
    }
    if (del) {
        /* d: 删除指定成员 (basename 匹配, 支持 @listfile 名字列表) */
        for (i = i_obj; i < argc; i++) {
            const char *a = argv[i];
            if (*a == '-')
                continue;
            if (a[0] == '@' && a[1]) {
                struct ar_ctx_del cd = { objs, &nobjs };
                if (ar_listfile_each(a + 1, ar_del_cb, &cd) < 0) {
                    fprintf(stderr, "tcc: ar: can't open listfile %s\n", a + 1);
                    goto the_end;
                }
                continue;
            }
            ar_del_member(objs, &nobjs, a);
        }
    } else {
        /* r/新建: 读入对象, 同名替换或追加, 支持 @listfile */
        for (i = i_obj; i < argc; i++) {
            const char *a = argv[i];
            if (*a == '-')
                continue;
            if (a[0] == '@' && a[1]) {
                struct ar_ctx_add ca = { &objs, &nobjs, verbose };
                if (ar_listfile_each(a + 1, ar_add_cb, &ca) < 0) {
                    fprintf(stderr, "tcc: ar: can't open listfile %s\n", a + 1);
                    goto the_end;
                }
                continue;
            }
            if (ar_add_member(&objs, &nobjs, a, verbose) < 0) {
                fprintf(stderr, "tcc: ar: can't open file %s\n", a);
                goto the_end;
            }
        }
    }

    sprintf(tfile, "%s.tmp", argv[i_lib]);
    if ((fo = fopen(tfile, "wb+")) == NULL)
    {
        fprintf(stderr, "tcc: ar: can't create temporary file %s\n", tfile);
        goto the_end;
    }

    funcmax = 250;
    afpos = tcc_realloc(NULL, funcmax * sizeof *afpos); // 250 func
    memcpy(&arhdro.ar_mode, "100644", 6);

    // write members from in-memory list
    for (j = 0; j < nobjs; j++)
    {
        ArMember *m = &objs[j];
        fsize = m->size;
        buf = m->data;

        if (verbose)
            printf("%s - %s\n", del ? "d" : "a", m->name);

        // elf header validity
        ehdr = (ElfW(Ehdr) *)(void *)buf;
        if (ehdr->e_ident[4] != ELFCLASSW)
        {
            fprintf(stderr, "tcc: ar: Unsupported Elf Class: %s\n", m->name);
            goto the_end;
        }

        shdr = (ElfW(Shdr) *) (buf + ehdr->e_shoff + ehdr->e_shstrndx * ehdr->e_shentsize);
        shstr = (char *)(buf + shdr->sh_offset);
        symtab = strtab = NULL;
        for (i = 0; i < ehdr->e_shnum; i++)
        {
            shdr = (ElfW(Shdr) *) (buf + ehdr->e_shoff + i * ehdr->e_shentsize);
            if (!shdr->sh_offset)
                continue;
            if (shdr->sh_type == SHT_SYMTAB)
            {
                symtab = (char *)(buf + shdr->sh_offset);
                symtabsize = shdr->sh_size;
            }
            if (shdr->sh_type == SHT_STRTAB)
            {
                if (!strcmp(shstr + shdr->sh_name, ".strtab"))
                {
                    strtab = (char *)(buf + shdr->sh_offset);
                }
            }
        }

        if (symtab && strtab)
        {
            int nsym = symtabsize / sizeof(ElfW(Sym));
            for (i = 1; i < nsym; i++)
            {
                sym = (ElfW(Sym) *) (symtab + i * sizeof(ElfW(Sym)));
                if (sym->st_shndx &&
                    (sym->st_info == 0x10
                    || sym->st_info == 0x11
                    || sym->st_info == 0x12
                    || sym->st_info == 0x20
                    || sym->st_info == 0x21
                    || sym->st_info == 0x22
                    )) {
                    istrlen = strlen(strtab + sym->st_name)+1;
                    anames = tcc_realloc(anames, strpos+istrlen);
                    strcpy(anames + strpos, strtab + sym->st_name);
                    strpos += istrlen;
                    if (++funccnt >= funcmax) {
                        funcmax += 250;
                        afpos = tcc_realloc(afpos, funcmax * sizeof *afpos); // 250 func more
                    }
                    afpos[funccnt] = fpos;
                }
            }
        }

        name = m->name;
        istrlen = strlen(name);
        memset(arhdro.ar_name, ' ', sizeof(arhdro.ar_name));
        if (istrlen < sizeof(arhdro.ar_name) - 1) {
            memcpy(arhdro.ar_name, name, istrlen);
            arhdro.ar_name[istrlen] = '/';
        } else {
            /* 长名: 存入 "//" 字符串表, 成员名记为 "/<offset>"; 读取端据此还原 */
            int off = longnames.size;
            char tmp[24];
            cstr_cat(&longnames, name, istrlen);
            cstr_cat(&longnames, "/\n", 2);
            longsize = longnames.size;
            snprintf(tmp, sizeof tmp, "/%d", off);
            memcpy(arhdro.ar_name, tmp, strlen(tmp));
        }
        sprintf(stmp, "%-10d", fsize);
        memcpy(&arhdro.ar_size, stmp, 10);
        fwrite(&arhdro, sizeof(arhdro), 1, fo);
        fwrite(buf, fsize, 1, fo);
        fpos += (fsize + sizeof(arhdro));
        if (fpos & 1)
            fputc(0, fo), ++fpos;
    }
    /* 仅在所有成员处理成功后才打开并截断目标: 失败时保留原归档 */
    if ((fh = fopen(argv[i_lib], "wb")) == NULL)
    {
        fprintf(stderr, "tcc: ar: can't create file %s\n", argv[i_lib]);
        goto the_end;
    }
    created_file = argv[i_lib];
    hofs = 8 + sizeof(arhdr) + strpos + (funccnt+1) * sizeof(int);
    /* 符号表区 (header+count+names) 需 2 字节对齐 */
    if ((hofs & 1)) // align
        hofs++, sympad = 1;
    if (longsize) {
        /* 长名表 "//" 伪成员同样计入对象基址: header + 数据(+奇数补1) */
        lnpad = longsize & 1;
        hofs += sizeof(arhdr) + longsize + lnpad;
    }
    // write header
    fwrite(ARMAG, 8, 1, fh);
    // create an empty archive
    if (!funccnt) {
        ret = 0;
        goto the_end;
    }
    sprintf(stmp, "%-10d", (int)(strpos + (funccnt+1) * sizeof(int)));
    memcpy(&arhdr.ar_size, stmp, 10);
    fwrite(&arhdr, sizeof(arhdr), 1, fh);
    afpos[0] = le2belong(funccnt);
    for (i=1; i<=funccnt; i++)
        afpos[i] = le2belong(afpos[i] + hofs);
    fwrite(afpos, (funccnt+1) * sizeof(int), 1, fh);
    fwrite(anames, strpos, 1, fh);
    if (sympad)
        fputc('\n', fh);
    /* write longname table "//" pseudo member */
    if (longsize) {
        memset(arhdr.ar_name, ' ', sizeof(arhdr.ar_name));
        arhdr.ar_name[0] = '/';
        arhdr.ar_name[1] = '/';
        sprintf(stmp, "%-10d", longsize + lnpad);
        memcpy(&arhdr.ar_size, stmp, 10);
        fwrite(&arhdr, sizeof(arhdr), 1, fh);
        fwrite(longnames.data, longsize, 1, fh);
        if (lnpad)
            fputc('\n', fh);
    }
    // write objects
    fseek(fo, 0, SEEK_END);
    fsize = ftell(fo);
    fseek(fo, 0, SEEK_SET);
    buf = tcc_malloc(fsize + 1);
    fread(buf, fsize, 1, fo);
    fwrite(buf, fsize, 1, fh);
    tcc_free(buf);
    ret = 0;
the_end:
    cstr_free(&longnames);
    for (i = 0; i < nobjs; i++) {
        tcc_free(objs[i].data);
        tcc_free(objs[i].name);
    }
    tcc_free(objs);
    if (anames)
        tcc_free(anames);
    if (afpos)
        tcc_free(afpos);
    if (fh)
        fclose(fh);
    if (created_file && ret != 0)
        remove(created_file);
    if (fo)
        fclose(fo), remove(tfile);
    return ret;
}

/* -------------------------------------------------------------- */
/*
 * tiny_impdef creates an export definition file (.def) from a dll
 * on MS-Windows. Usage: tiny_impdef library.dll [-o outputfile]"
 *
 *  Copyright (c) 2005,2007 grischka
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef TCC_TARGET_PE

ST_FUNC int tcc_tool_impdef(int argc, char **argv)
{
    int ret, v, i;
    char infile[260];
    char outfile[260];

    const char *file;
    char *p, *q;
    FILE *fp, *op;

#if defined(_WIN32) && !defined(CONFIG_TCC_MUSL)
    char path[260];
#endif

    infile[0] = outfile[0] = 0;
    fp = op = NULL;
    ret = 1;
    p = NULL;
    v = 0;

    for (i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if ('-' == a[0]) {
            if (0 == strcmp(a, "-v")) {
                v = 1;
            } else if (0 == strcmp(a, "-o")) {
                if (++i == argc)
                    goto usage;
                strcpy(outfile, argv[i]);
            } else
                goto usage;
        } else if (0 == infile[0])
            strcpy(infile, a);
        else
            goto usage;
    }

    if (0 == infile[0]) {
usage:
        fprintf(stderr,
            "usage: tcc -impdef library.dll [-v] [-o outputfile]\n"
            "create export definition file (.def) from dll\n"
            );
        goto the_end;
    }

    if (0 == outfile[0]) {
        strcpy(outfile, tcc_basename(infile));
        q = strrchr(outfile, '.');
        if (NULL == q)
            q = strchr(outfile, 0);
        strcpy(q, ".def");
    }

    file = infile;
#if defined(_WIN32) && !defined(CONFIG_TCC_MUSL)
    if (SearchPath(NULL, file, ".dll", sizeof path, path, NULL))
        file = path;
#endif
    ret = tcc_get_dllexports(file, &p);
    if (ret || !p) {
        fprintf(stderr, "tcc: impdef: %s '%s'\n",
            ret == -1 ? "can't find file" :
            ret ==  1 ? "can't read symbols" :
            ret ==  0 ? "no symbols found in" :
            "unknown file type", file);
        ret = 1;
        goto the_end;
    }

    if (v)
        printf("-> %s\n", file);

    op = fopen(outfile, "wb");
    if (NULL == op) {
        fprintf(stderr, "tcc: impdef: could not create output file: %s\n", outfile);
        goto the_end;
    }

    fprintf(op, "LIBRARY %s\n\nEXPORTS\n", tcc_basename(file));
    for (q = p, i = 0; *q; ++i) {
        fprintf(op, "%s\n", q);
        q += strlen(q) + 1;
    }

    if (v)
        printf("<- %s (%d symbol%s)\n", outfile, i, &"s"[i<2]);

    ret = 0;

the_end:
    if (p)
        tcc_free(p);
    if (fp)
        fclose(fp);
    if (op)
        fclose(op);
    return ret;
}

#endif /* TCC_TARGET_PE */

/* -------------------------------------------------------------- */
/*
 *  TCC - Tiny C Compiler
 *
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* re-execute the i386/x86_64 cross-compilers with tcc -m32/-m64: */

#if !defined TCC_TARGET_I386 && !defined TCC_TARGET_X86_64

ST_FUNC int tcc_tool_cross(char **argv, int option)
{
    fprintf(stderr, "tcc -m%d not implemented\n", option);
    return 1;
}

#else
#if defined(_WIN32) && !defined(CONFIG_TCC_MUSL)
#include <process.h>

/* - Empty argument or with space/tab (not newline) requires quoting.
 * - Double-quotes at the value require '\'-escape, regardless of quoting.
 * - Consecutive (or 1) backslashes at the value all need '\'-escape only if
 *   followed by [escaped] double quote, else taken literally, e.g. <x\\y\>
 *   remains literal without quoting or esc, but <x\\"y\> becomes <x\\\\\"y\>.
 * - This "before double quote" rule applies also before delimiting quoting,
 *   e.g. <x\y \"z\> becomes <"x\y \\\"z\\"> (quoting required because space).
 *
 * https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments
 */
static char *quote_win32(const char *s)
{
    char *o, *r = tcc_malloc(2 * strlen(s) + 3);   /* max-esc, quotes, \0 */
    int cbs = 0, quoted = !*s;  /* consecutive backslashes before current */

    for (o = r; *s; *o++ = *s++) {
        quoted |= *s == ' ' || *s == '\t';
        if (*s == '\\' || *s == '"')
            *o++ = '\\';
        else
            o -= cbs;  /* undo cbs escapes, if any (not followed by DQ) */
        cbs = *s == '\\' ? cbs + 1 : 0;
    }
    if (quoted) {
        memmove(r + 1, r, o++ - r);
        *r = *o++ = '"';
    } else {
        o -= cbs;
    }

    *o = 0;
    return r; /* don't bother with realloc(r, o-r+1) */
}

static int execvp_win32(const char *prog, char **argv)
{
    int ret; char **p;
    /* replace all " by \" */
    for (p = argv; *p; ++p)
        *p = quote_win32(*p);
    ret = _spawnvp(P_NOWAIT, prog, (const char *const*)argv);
    if (-1 == ret)
        return ret;
    _cwait(&ret, ret, WAIT_CHILD);
    exit(ret);
}
#define execvp execvp_win32
#endif /* _WIN32 */

ST_FUNC int tcc_tool_cross(char **argv, int target)
{
    char program[4096];
    char *a0 = argv[0];
    int prefix = tcc_basename(a0) - a0;

    snprintf(program, sizeof program,
        "%.*s%s"
#ifdef TCC_TARGET_PE
        "-win32"
#endif
        "-tcc"
#ifdef _WIN32
        ".exe"
#endif
        , prefix, a0, target == 64 ? "x86_64" : "i386");

    if (strcmp(a0, program))
        execvp(argv[0] = program, argv);
    fprintf(stderr, "tcc: could not run '%s'\n", program);
    return 1;
}

/* re-execute the win/linux tcc with tcc -platform=win|linux:
   tcc_posix ships two single-target compilers (tcc-win.exe PE,
   tcc-linux.exe ELF).  -platform selects which one to run. */
ST_FUNC int tcc_tool_platform(char **argv, const char *platform)
{
    char program[4096];
    char *a0 = argv[0];
    const char *base = tcc_basename(a0);
    int prefix = base - a0;
    int want_win;

    if (!platform || (*platform != 'w' && *platform != 'l')) {
        fprintf(stderr, "tcc: -platform must be win or linux\n");
        return 1;
    }
    want_win = (*platform == 'w');

    /* if already the requested platform, proceed normally (ret != 0 -> continue) */
    if ((want_win && strstr(base, "-win"))
        || (!want_win && strstr(base, "-linux")))
        return -1;

    snprintf(program, sizeof program,
        "%.*stcc-%s%s", prefix, a0,
        want_win ? "win" : "linux",
#ifdef _WIN32
        ".exe"
#else
        ""
#endif
        );

    if (strcmp(a0, program))
        execvp(argv[0] = program, argv);
    fprintf(stderr, "tcc: could not run '%s'\n", program);
    return 1;
}

#endif /* TCC_TARGET_I386 && TCC_TARGET_X86_64 */
/* -------------------------------------------------------------- */
/* enable commandline wildcard expansion (tcc -o x.exe *.c) */

#if defined(_WIN32) && !defined(CONFIG_TCC_MUSL)
const int _CRT_glob = 1;
#ifndef _CRT_glob
const int _dowildcard = 1;
#endif
#endif

/* -------------------------------------------------------------- */
/* generate xxx.d file */

static char *escape_target_dep(const char *s) {
    char *res = tcc_malloc(strlen(s) * 2 + 1);
    int j;
    for (j = 0; *s; s++, j++) {
        if (is_space(*s)) {
            res[j++] = '\\';
        }
        res[j] = *s;
    }
    res[j] = '\0';
    return res;
}

ST_FUNC int gen_makedeps(TCCState *s1, const char *target, const char *filename)
{
    FILE *depout;
    char buf[1024];
    char **escaped_targets;
    int i, k, num_targets;

    if (!filename) {
        /* compute filename automatically: dir/file.o -> dir/file.d */
        snprintf(buf, sizeof buf, "%.*s.d",
            (int)(tcc_fileextension(target) - target), target);
        filename = buf;
    }

    if(!strcmp(filename, "-"))
        depout = fdopen(1, "w");
    else
        /* XXX return err codes instead of error() ? */
        depout = fopen(filename, "w");
    if (!depout)
        return tcc_error_noabort("could not open '%s'", filename);
    if (s1->verbose)
        printf("<- %s\n", filename);

    escaped_targets = tcc_malloc(s1->nb_target_deps * sizeof(*escaped_targets));
    num_targets = 0;
    for (i = 0; i<s1->nb_target_deps; ++i) {
        for (k = 0; k < i; ++k)
            if (0 == strcmp(s1->target_deps[i], s1->target_deps[k]))
                goto next;
        escaped_targets[num_targets++] = escape_target_dep(s1->target_deps[i]);
    next:;
    }

    fprintf(depout, "%s:", target);
    for (i = 0; i < num_targets; ++i)
        fprintf(depout, " \\\n  %s", escaped_targets[i]);
    fprintf(depout, "\n");
    if (s1->gen_phony_deps) {
        /* Skip first file, which is the c file.
         * Only works for single file give on command-line,
         * but other compilers have the same limitation */
        for (i = 1; i < num_targets; ++i)
            fprintf(depout, "%s:\n", escaped_targets[i]);
    }
    for (i = 0; i < num_targets; ++i)
        tcc_free(escaped_targets[i]);
    tcc_free(escaped_targets);
    fclose(depout);
    return 0;
}

/* -------------------------------------------------------------- */
