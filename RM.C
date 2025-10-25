
//1. 头文件和宏定义
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BASEXP 6
#define BASE 1000000
#define LMAX 200

typedef unsigned long long tipo;

typedef struct {
    int l;          // 长度（数字块数）
    tipo n[LMAX];   // 数组，存储数字块，低位在前
} Long;


//2. Long初始化和辅助函数
// 初始化Long为0
void Long_init(Long *a) {
    a->l = 1;
    memset(a->n, 0, sizeof(tipo)*LMAX);
}

// 从tipo初始化Long
void Long_from_tipo(Long *a, tipo x) {
    Long_init(a);
    int i = 0;
    while (x > 0 && i < LMAX) {
        a->n[i++] = x % BASE;
        x /= BASE;
    }
    a->l = (i == 0) ? 1 : i;
}

// 从字符串初始化Long，字符串为数字字符，最高位在前
void Long_from_str(Long *a, const char *s) {
    Long_init(a);
    int len = (int)strlen(s);
    a->l = (len + BASEXP - 1) / BASEXP;
    memset(a->n, 0, sizeof(tipo)*LMAX);

    tipo r = 1;
    for (int i = 0; i < len; i++) {
        int digit = s[len - 1 - i] - '0';
        a->n[i / BASEXP] += digit * r;
        r *= 10;
        if (r == BASE) r = 1;
    }
    // 去除高位0
    while (a->l > 1 && a->n[a->l - 1] == 0) a->l--;
}


//3. 输出Long
void Long_print(const Long *a) {
    printf("%llu", a->n[a->l - 1]);
    for (int i = a->l - 2; i >= 0; i--) {
        printf("%06llu", a->n[i]);
    }
    printf("\n");
}

//4. 归一化函数（去除高位0）
void Long_invar(Long *a) {
    while (a->l > 1 && a->n[a->l - 1] == 0) a->l--;
    for (int i = a->l; i < LMAX; i++) a->n[i] = 0;
}

//5. 比较函数
// a < b ?
int Long_less(const Long *a, const Long *b) {
    if (a->l != b->l) return a->l < b->l;
    for (int i = a->l - 1; i >= 0; i--) {
        if (a->n[i] != b->n[i]) return a->n[i] < b->n[i];
    }
    return 0;
}

// a == b ?
int Long_equal(const Long *a, const Long *b) {
    if (a->l != b->l) return 0;
    for (int i = 0; i < a->l; i++) {
        if (a->n[i] != b->n[i]) return 0;
    }
    return 1;
}

// a <= b ?
int Long_less_equal(const Long *a, const Long *b) {
    return Long_less(a,b) || Long_equal(a,b);
}


//6. 加法
void Long_add(const Long *a, const Long *b, Long *c) {
    int maxl = (a->l > b->l) ? a->l : b->l;
    tipo carry = 0;
    for (int i = 0; i < maxl; i++) {
        tipo aval = (i < a->l) ? a->n[i] : 0;
        tipo bval = (i < b->l) ? b->n[i] : 0;
        tipo sum = aval + bval + carry;
        c->n[i] = sum % BASE;
        carry = sum / BASE;
    }
    c->l = maxl;
    if (carry) {
        if (c->l < LMAX) c->n[c->l++] = carry;
    }
    Long_invar(c);
}

void Long_add_inplace(Long *a, const Long *b) {
    Long c;
    Long_add(a, b, &c);
    *a = c;
}


//7. 减法（假设a >= b）
// c = a - b, 返回1表示成功，0表示a < b（不支持负数）
int Long_sub(const Long *a, const Long *b, Long *c) {
    if (Long_less(a, b)) return 0;
    tipo borrow = 0;
    for (int i = 0; i < a->l; i++) {
        tipo aval = a->n[i];
        tipo bval = (i < b->l) ? b->n[i] : 0;
        tipo diff = aval - bval - borrow;
        if ((long long)diff < 0) {
            diff += BASE;
            borrow = 1;
        } else {
            borrow = 0;
        }
        c->n[i] = diff;
    }
    c->l = a->l;
    Long_invar(c);
    return 1;
}

void Long_sub_inplace(Long *a, const Long *b) {
    Long c;
    Long_sub(a, b, &c);
    *a = c;
}


/*8. 乘法
注意： 这里用到了unsigned __int128，这是GCC/Clang支持的128位整数类型，
用于防止乘法溢出。如果你的编译器不支持，需要用其他方法实现乘法（比如分块乘法）
*/
void Long_mul(const Long *a, const Long *b, Long *c) {
    memset(c->n, 0, sizeof(tipo)*LMAX);
    for (int i = 0; i < a->l; i++) {
        tipo carry = 0;
        for (int j = 0; j < b->l; j++) {
            if (i + j >= LMAX) break;
            unsigned __int128 mul = (unsigned __int128)a->n[i] * b->n[j] + c->n[i + j] + carry;
            c->n[i + j] = (tipo)(mul % BASE);
            carry = (tipo)(mul / BASE);
        }
        if (i + b->l < LMAX) c->n[i + b->l] += carry;
    }
    c->l = a->l + b->l;
    if (c->l > LMAX) c->l = LMAX;
    Long_invar(c);
}

void Long_mul_inplace(Long *a, const Long *b) {
    Long c;
    Long_mul(a, b, &c);
    *a = c;
}

//9. 乘以tipo（long long）
void Long_mul_tipo(const Long *a, tipo b, Long *c) {
    tipo carry = 0;
    for (int i = 0; i < a->l; i++) {
        unsigned __int128 mul = (unsigned __int128)a->n[i] * b + carry;
        c->n[i] = (tipo)(mul % BASE);
        carry = (tipo)(mul / BASE);
    }
    c->l = a->l;
    while (carry && c->l < LMAX) {
        c->n[c->l++] = carry % BASE;
        carry /= BASE;
    }
    Long_invar(c);
}

void Long_mul_tipo_inplace(Long *a, tipo b) {
    Long c;
    Long_mul_tipo(a, b, &c);
    *a = c;
}

//10. 除以tipo，返回商和余数
void Long_div_tipo(const Long *a, tipo b, Long *c, tipo *rm) {
    *rm = 0;
    for (int i = a->l - 1; i >= 0; i--) {
        unsigned __int128 cur = (unsigned __int128)(*rm) * BASE + a->n[i];
        c->n[i] = (tipo)(cur / b);
        *rm = (tipo)(cur % b);
    }
    c->l = a->l;
    Long_invar(c);
}
//11. 除法和取模（Long除Long）
/*这个比较复杂，且在Rabin-Miller中只用到n-1除以2的操作，可以用专门的除以2函数代替*/
void Long_div_tipo_inplace(Long *a, tipo b, tipo *rm) {
    Long c;
    Long_div_tipo(a, b, &c, rm);
    *a = c;
}
//12. 除以2函数（用于指数分解）
// 判断是否偶数
int Long_is_even(const Long *a) {
    return (a->n[0] % 2) == 0;
}

// 除以2
void Long_div2(const Long *a, Long *c) {
    tipo carry = 0;
    for (int i = a->l - 1; i >= 0; i--) {
        unsigned __int128 cur = (unsigned __int128)carry * BASE + a->n[i];
        c->n[i] = (tipo)(cur / 2);
        carry = (tipo)(cur % 2);
    }
    c->l = a->l;
    Long_invar(c);
}
//13. 减1函数

void Long_sub_one(Long *a) {
    tipo borrow = 1;
    for (int i = 0; i < a->l && borrow; i++) {
        if (a->n[i] >= borrow) {
            a->n[i] -= borrow;
            borrow = 0;
        } else {
            a->n[i] = BASE + a->n[i] - borrow;
            borrow = 1;
        }
    }
    Long_invar(a);
}

//14. gcd函数
void Long_gcd(Long a, Long b, Long *res) {
    Long zero;
    Long_init(&zero);
    Long t;
    while (!Long_equal(&b, &zero)) {
        // t = b
        t = b;
        // b = a % b
        // 这里a % b用模运算函数实现，暂时用简单减法循环（效率低）
        // 由于b可能很大，建议实现模运算函数
        // 这里先用简单方法，后面可优化
        // 先实现Long_mod
        Long_mod(&a, &b, &b);
        a = t;
    }
    *res = a;
}

/*
15. Long模运算（a % b）
模运算是除法的副产品，比较复杂。为了简化，考虑：

Rabin-Miller中模运算用得较多，必须实现。
这里给出一个简单的减法模实现（效率低），适合小测试。
*/
// 简单模运算，a % b，效率低
void Long_mod(const Long *a, const Long *b, Long *res) {
    Long temp = *a;
    while (!Long_less(&temp, b)) {
        Long_sub_inplace(&temp, b);
    }
    *res = temp;
}
//16. 模幂运算

void Long_modexp(const Long *a, const Long *b, const Long *n, Long *res) {
    Long base = *a;
    Long exp = *b;
    Long one;
    Long_init(&one);
    one.n[0] = 1;

    Long c;
    Long_init(&c);
    c.n[0] = 1;

    Long zero;
    Long_init(&zero);

    while (!Long_equal(&exp, &zero)) {
        if (!Long_is_even(&exp)) {
            Long_mul(&c, &base, &c);
            Long_mod(&c, n, &c);
            Long_sub_one(&exp);
        } else {
            Long_mul(&base, &base, &base);
            Long_mod(&base, n, &base);
            Long_div2(&exp, &exp);
        }
    }
    *res = c;
}

//17. Rabin-Miller测试
int Long_isprime(const Long *n, int rounds) {
    Long two;
    Long_init(&two);
    two.n[0] = 2;

    Long one;
    Long_init(&one);
    one.n[0] = 1;

    Long zero;
    Long_init(&zero);

    if (Long_equal(n, &two)) return 1;
    if (Long_equal(n, &one)) return 0;

    // 如果n是偶数，返回0
    if (!Long_is_even(n)) {
        // 奇数，继续
    } else {
        return 0;
    }

    // 写 n-1 = d * 2^s
    Long d;
    Long_sub(n, &one, &d);
    int s = 0;
    Long temp_d = d;
    while (Long_is_even(&temp_d)) {
        Long_div2(&temp_d, &temp_d);
        s++;
    }
    d = temp_d;

    srand((unsigned int)time(NULL));

    for (int i = 0; i < rounds; i++) {
        // 生成随机基b，1 < b < n-1
        Long b;
        do {
            Long_init(&b);
            for (int j = 0; j < n->l; j++) {
                b.n[j] = (tipo)rand() % BASE;
            }
            b.l = n->l;
            Long_invar(&b);
        } while (Long_less_equal(&b, &one) || !Long_less(&b, n));

        // gcd(b, n) == 1 ?
        Long g;
        Long_gcd(b, *n, &g);
        if (!Long_equal(&g, &one)) {
            if (Long_equal(&b, n)) continue;
            else return 0;
        }

        Long x;
        Long_modexp(&b, &d, n, &x);

        if (Long_equal(&x, &one) || Long_equal(&x, &d)) continue;

        int cont_flag = 0;
        for (int r = 1; r < s; r++) {
            Long_mul(&x, &x, &x);
            Long_mod(&x, n, &x);
            if (Long_equal(&x, &d)) {
                cont_flag = 1;
                break;
            }
            if (Long_equal(&x, &one)) return 0;
        }
        if (cont_flag) continue;

        return 0;
    }
    return 1;
}


int main() {
    int t;
    scanf("%d", &t);
    char s[LMAX * BASEXP + 10];
    for (int i = 0; i < t; i++) {
        scanf("%s", s);
        Long N;
        Long_from_str(&N, s);
        if (Long_isprime(&N, 10)) {
            printf("YES\n");
        } else {
            printf("NO\n");
        }
    }
    return 0;
}
/*
代码中模运算和除法部分用的是简单减法循环，效率极低，
实际大数测试时需要实现更高效的除法和模运算算法。
*/