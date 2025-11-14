#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#define bignum unsigned long long
#define uint128_t unsigned long
#define __uint128_t unsigned long
// 快速幂取模：计算 (base^exp) % mod
//mod 待测数 base为测试基数 exp =mod -1
uint64_t mod_exp(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t result = 1;
    base %= mod;
    while (exp > 0) {
        if (exp & 1)
            result = (__uint128_t)result * base % mod;  // 防止溢出，使用128位乘法
        base = (__uint128_t)base * base % mod;
        exp >>= 1;
    }
    return result;
}

// Miller-Rabin单轮测试，a为测试基数，n为待测数
int miller_rabin_test(uint64_t n, uint64_t a) {
    if (n % a == 0) return 0;

    uint64_t d = n - 1;
    uint64_t r = 0;

    // 将 n-1 写成 2^r * d
    while ((d & 1) == 0) {
        d >>= 1;
        r++;
    }

    uint64_t x = mod_exp(a, d, n);
    if (x == 1 || x == n - 1)
        return 1;

    for (uint64_t i = 0; i < r - 1; i++) {
        x = (__uint128_t)x * x % n;
        if (x == n - 1)
            return 1;
    }

    return 0;  // 合数
}

// Miller-Rabin主函数，k为测试轮数，越大准确率越高
int is_prime(uint64_t n, int k) {
    if (n < 2) return 0;
    if (n == 2 || n == 3) return 1;
    if ((n & 1) == 0) return 0;  // 偶数直接排除

    // 选择k个随机基数进行测试
    srand((unsigned)time(NULL));
    for (int i = 0; i < k; i++) {
        uint64_t a = 2 + rand() % (n - 3);
        if (!miller_rabin_test(n, a))
            return 0;  // 发现合数
    }
    return 1;  // 可能是素数
}

// 测试示例
int main() {
    //uint64_t test_nums[] = {2, 3, 17, 19, 20, 561, 1105, 1729, 2147483647, 9223372036854775783ULL};
    int k = 10;  // 测试轮数

    // for (int i = 0; i < sizeof(test_nums)/sizeof(test_nums[0]); i++) {
    //     uint64_t n = test_nums[i];
    uint64_t n ;
    scanf("%d",&n); //656565632653265651;
        if (is_prime(n, k))
            printf("%llu 是素数（概率判断）\n", n);
        else
            printf("%llu 不是素数\n", n);
    // }

    return 0;
}


int is_prime(x,k)   {
    //x <2 或偶数直接返回
    //根据 k 创随机数进行打印并测试
    //测试结果 返回
}

int main(){
    //先读取键盘输入待测数x，定义测试轮数k
    //调用判断函数(is_prime(x,k))根据返回值 写入文件
    //
}
//a为随机生成数，n为待测数，q为次方，k为平方次数

char Miller_Rabin_judge(bignum a, bignum n, bignum q, int k)
{
    uint128_t aq = 1;
    a %= n;     //是不是有需要这一步
    while(q >= 1){
        if(q & 1){
            aq =(aq * a)% n;
        }
        a=(a* a)% n;
        q >>= 1;
    }
    if(aq == 1){
        return 0;
    }
    for(short i= 1;i< k; i++){
        aq =(aq * aq)% n;
        if(aq ==n-1|| aq == 1){
            return 0;
        }
    }
    return 1;
}