### 컴파일

```bash
$ gcc -c forkas.S -o forkas.o

$ gcc -O2 -c target.c

$ gcc target.o forkas.o -o static_link_target
```

static_link에서만 이런 식으로 컴파일

```bash
$ gcc <binary_name> <code>
```
나머지는 동일

### 사용방법
```bash
$./fork_DEBUG <path/target> <seed>
```

