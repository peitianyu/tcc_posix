# __chkstk: tcc win64 栈探测/分配 (约定: eax = 帧大小)
# TCC 生成: mov <framesize>,%eax; call __chkstk  (替代 push rbp; mov rbp,rsp; sub)
# 布局: rbp = 入口rsp-8; [rbp] = caller rbp; [rbp+8] = 返回地址
# 返回: jmp *8(%rbp) (rsp 已移动, 不能 ret); TCC 函数尾 leave;ret 依赖此布局
.section .text
.global __chkstk
__chkstk:
	sub	$8,%rsp
	mov	%rbp,(%rsp)
	mov	%rsp,%rbp
	mov	%eax,%ecx
	add	$15,%ecx
	and	$-16,%ecx
	lea	-8(%rbp),%rax
	cmp	$0x1000,%ecx
	jbe	chk_done
chk_loop:
	sub	$0x1000,%rax
	test	%eax,(%rax)
	sub	$0x1000,%ecx
	cmp	$0x1000,%ecx
	ja	chk_loop
chk_done:
	sub	%rcx,%rax
	test	%eax,(%rax)
	mov	%rax,%rsp
	mov	8(%rbp),%rcx
	jmp	*%rcx
