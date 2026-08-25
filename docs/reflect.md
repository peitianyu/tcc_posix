# 结构体反射 (struct reflection) 设计方案

> 状态: 2026-08-23 设计 → **P0 已实现并回归 (t051, 套件 79/79)**. ABI 以 lib/tcc-reflect.h
>       与 tccgen.c 的 refl_emit 为准.
> 依据: 「反射器是否契合 TCC 单遍哲学」讨论——结论: 反射是少有的、**天然单遍友好**
>       的编译器能力, 推荐在通用 comptime 之前立项 (comptime 需解释器, 撞哲学)。
> 目标: 让任意 struct/union/类型能在编译期得到「字段名/类型/偏移/大小/对齐」元数据,
>       用于序列化/调试/DSL/内存治理展示, 契合 TCC「少丢信息、不多建中间层」的定位。

---

## 1. 目标与定位

C 无内省。反射让 `for field in T` 变成可能:
```c
const struct __refl *r = __reflect(struct Vec3);
for (int i = 0; i < r->nfield; i++)
    printf("%s @%u +%u %uB\n", r->fields[i].name, r->fields[i].offset,
           r->fields[i].size, r->fields[i].align);
```

- **定位**: 与 `-b`/memtrack/bt 一路——**把 TCC 解析时本就持有、却随手丢弃的字段信息
  (名字/类型/offset) 保留成一张静态表**; 不建 AST、不加第二遍、不需解释器, 单遍内完成。
- 与整体路线关系: 可配合 C.6 脱糖输出端做序列化; 与 operator/model/matrix 正交。

## 2. 用户侧 API (语法)

采用内建 `__reflect(Type)`, 复用 TCC 内建分发与 `parse_type`:

- `__reflect(T)` → `const struct __refl *`, `T` 为任意类型名
  (`struct Vec3` / `union U` / `typedef 名` / 标量 / 指针)。
- 元信息由编译器在 `.rdata` 生成静态常量表, 返回其地址。
- 内建优势: 不占保留字、与 `_mm_*`/`__builtin_*` 同一套机制、可后续扩展
  (`__reflect_name`/`__reflect_fields(T)` 等)。

## 3. 元数据表示 (生成物)

```c
typedef enum __refl_kind {
    RE_KIND_STRUCT=1, RE_KIND_UNION, RE_KIND_PTR,
    RE_KIND_INT, RE_KIND_FLOAT, RE_KIND_LLONG, RE_KIND_BYTE,
    RE_KIND_BOOL, RE_KIND_ENUM, RE_KIND_ARRAY, RE_KIND_VOID, ...
} __refl_kind;

typedef struct __refl_field {
    const char *name;    /* 字段名 */
    unsigned kind;       /* 字段的 __refl_kind */
    unsigned offset, size, align;
    const void *sub;     /* v2: 指向嵌套 struct 的 __refl*, 平铺期填 NULL */
} __refl_field;

typedef struct __refl {
    const char *name;      /* 类型名 (tag/typedef 字符串) */
    unsigned kind, size, align, nfield;
    const __refl_field *fields;   /* 平铺字段数组 (仅 struct/union 有意义) */
} __refl;
```

## 4. 编译器内实现

### 4.1 内建分发
在 `unary()` 的内建分发点 (与 `simd_builtin_dispatch`、`__builtin_*` 分支并列) 加
`__reflect`: 用 `parse_builtin_params(0, "t")` 走 `parse_type(&type)` 解析类型并压栈
(已支持 `t` 参数, tccgen.c L6261)。此时 vtop 为类型引用 (`type.ref` 指向 struct/union
符号), 据此生成元数据。

> 或单独 `next(); parse_type(&t); skip(')')` 更直接拿到 CType `t`, 不走通用压栈。

### 4.2 类型信息来源 (全部现成)
- 结构体符号: `type.ref` 为 `SYM_STRUCT`, 字段链从 `sym->next`... (字段 `Sym`, 每个
  `s->c` = offset, `s->type` = 字段类型, `s->v` 可 `get_tok_str` 得名字)。
- 总大小/对齐: `type_size(&type, &align)` (tccgen.c L3620), 结构体大小存
  `type.ref->c`。
- 字段 kind 编码: 由字段 `CType.t & VT_BTYPE` (VT_STRUCT/VT_PTR/VT_INT/VT_FLOAT/…)
  映射到 `__refl_kind`; `VT_BTYPE` 集合是现成的 (tcc.h)。

### 4.3 元数据生成 + 落 `.rdata`
`__reflect(T)` 首次遇到类型 `T` 时:
1. `new_section(state, ".rdata", ...)` 取只读段;
2. 用 `get_tok_str(field->v)` 把字段名写入段内 (记 offset), 字段 `name` 指针指向它;
3. 填充 `__refl_field[]` 数组与 `__refl` 头 (`section_ptr_add` 获得可写缓冲);
4. 以匿名符号登记 (`anon_sym` + `put_extern_sym`/`gen_extern_sym`), 返回 `vpush` 一个
   `const __refl*` 常量指针 (VT_CONST + sym, 类型为 `const struct __refl*`)。

### 4.4 注册 / 缓存
按 `type.ref` (符号指针) 做 key: 生成过的类型直接复用, 避免复制建表、也保证两处
`__reflect(struct Vec3)` 拿到同一地址。开放数组表 `/ 固定容量 + 零分配`(仿 memtrack/
cpu-prof 骨架)。

## 5. 复用既有机制 (方案成本低的原因)

| 机制 | 位置 | 用途 |
|---|---|---|
| `parse_builtin_params("t")` / `parse_type` | tccgen.c L6261 | 内建解析类型 |
| `struct_layout` 的 `field->c = offset` | tccgen.c L4314 | 字段偏移已就绪 |
| `type_size()` | tccgen.c L3620 | 总大小/对齐 |
| `field->type` / `field->v` | — | 字段类型编码 / 字段名 |
| `new_section`+`section_ptr_add`+`anon_sym`+`put_extern_sym` | tccelf | 写 `.rdata` 常量表 |
| `VT_BTYPE` 位集 | tcc.h | kind 编码 |

## 6. 支持范围与诚实边界 (MVP)

- **支持**: struct/union 平铺字段; 标量/指针/enum 字段给 kind+size/align; 类型总
  size/align; 类型名。
- **暂不做 (v2)**: bitfield、VLA、函数指针字段; 嵌套 struct 递归链 (`sub` 预留,
  平铺期先置 NULL, v2 再链接, 避免循环引用); 匿名 union/匿名成员。
- **语法面**: 仅 `__reflect(T)`, 不做 `for field` 遍历语法糖 (用户自己 while 遍历)。

## 7. 测试计划

`tests/t051_reflect.c` (单文件, 进 test.sh 自动遍历) 或 `build/tests/t_reflect.c`:
- 断言一个已知 struct 的 `nfield`、每个字段 `name`/`offset`/`size`/`align` 与手工
  `offsetof`/`sizeof` 一致;
- 遍历字段做"假序列化"demo (打印/按 offset 写 buffer), 验证反射驱动访问;
- union 与 typedef 标量的 size/align;
- 两处 `__reflect` 同类型地址一致 (缓存)。

## 8. 分阶段

| 阶段 | 产出 | 验收 |
|---|---|---|
| P0 | 内建 `__builtin_reflect` + struct/union/scalar 平铺元数据落 `.rdata` | ✅ 已实现; t051 PASS (字段名/offset/size/align/kind) |
| P1 | 嵌套 struct 链接 (sub), 递归/多级; array 字段 | ✅ 已实现: __refl_field 扩到 32B(+sub), refl_emit 返回段偏移递归生成子表(深优先), 表起点 8 对齐, -1 哨兵表示"无子表"; t051 断言 Node.v->Vec3 |
| P2 | 序列化 demo; 与 C.6 脱糖输出端 / 调试器配合 | ✅ 序列化 demo: t051 的 refl_copy 按反射递归深拷贝(嵌套经 sub) |

---

## 9. 与整体路线关系

- 反射是**前几轮评估里唯一"单遍哲学不冲突 + 建议优先"**的编译器新特性
  (对比通用 comptime 需解释器/撞哲学)。
- 放在 `operator`(已实现) 同类——作为「少丢信息型」的语言扩展; 不依赖 AST/IR,
  与脱糖输出端 (C.6) 正交且能配合。