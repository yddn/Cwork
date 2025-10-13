#include    <stdio.h>
#include    <stdarg.h>
#include    "loc_incl.h"

int scanf(const char *format, ...)
{
    va_list ap;
    int retval;

    va_start(ap, format);

    retval = _doscan(stdin, format, ap);

    va_end(ap);

    return retval;
}
/*在mingw64->stdarg.h中：*/
/* Define va_list, if desired, from __gnuc_va_list. */
/* We deliberately do not define va_list when called from
   stdio.h, because ANSI C says that stdio.h is not supposed to define
   va_list.  stdio.h needs to have access to that data type, 
   but must not use that name.  It should use the name __gnuc_va_list,
   which is safe because it is reserved for the implementation.  */
/*这段代码实现了一个类似于标准库中scanf系列函数的核心解析函数，
名字叫_doscan。它从给定的文件流stream中读取数据，
根据格式字符串format解析输入，并将结果存储到可变参数列表ap中。
下面我详细解释这段代码的功能和流程：
函数功能概述
_doscan函数的作用是根据格式字符串format，
从输入流stream中读取数据，解析成对应的数据类型，
并存储到va_list ap指向的参数中。
它是实现scanf、fscanf等函数的底层核心。
主要变量说明
done：成功赋值的变量个数。
nrchars：已读取的字符数。
conv：转换次数（格式转换符的数量）。
base：数字转换时的进制（如10进制、16进制等）。
val：临时存储整数值。
str、tmp_string：临时字符串指针。
width：字段宽度限制。
flags：标志位，表示是否跳过赋值、长度修饰符等。
reverse：用于%[]格式，表示是否取反匹配。
kind：当前格式转换符的类型。
ic：当前读取的字符。
跳过格式字符串中的空白字符，并从输入流中跳过对应空白
这保证了格式字符串中的空白与输入流中的空白相匹配。

如果格式字符不是%，则直接匹配输入流中的对应字符
如果不匹配则退出。

遇到%%时，匹配输入流中的%字符。

解析格式修饰符
*表示跳过赋值（不写入参数）。
数字表示字段宽度限制。
h、l、L表示数据类型修饰符（short、long、long double）。
根据格式转换符类型读取数据
对于整数类型（d、i、o、u、x等），调用o_collect函数收集数字字符串，然后用strtol或strtoul转换成整数，存入对应参数。
对于字符类型c，直接读取指定宽度的字符。
对于字符串s，读取非空白字符直到宽度限制或遇到空白。
对于%[]，实现字符集匹配，支持取反匹配。
对于n，将已读取字符数写入对应参数。
对于浮点数（e、f、g等），调用f_collect收集浮点数字符串，使用strtod转换。
处理输入流字符的回退
如果读取多了字符，使用ungetc将多读的字符放回输入流。

更新计数器和状态
成功转换的次数conv加一。
如果不是跳过赋值，done加一。
循环继续直到格式字符串结束或遇到错误。

返回成功赋值的变量个数，若无转换且遇到EOF返回EOF。*/


int _doscan(register FILE *stream, const char *format, va_list ap)
{
  int done = 0;       /* number of items done */
  int nrchars = 0;    /* number of characters read */
  int conv = 0;       /* # of conversions */
  int base;           /* conversion base */
  unsigned long val;  /* an integer value */
  register char *str; /* temporary pointer */
  char *tmp_string;   /* ditto */
  unsigned width = 0; /* width of field */
  int flags;          /* some flags */
  int reverse;        /* reverse the checking in [...] */
  int kind;
  register int ic = EOF; /* the input character */
#ifndef NOFLOAT
  long double ld_val;
#endif

  if (!*format)   //格式字符串为空时直接返回0
    return 0;

  while (1)
  {
    if (isspace(*format))
    {
      while (isspace(*format))
        format++; /* skip whitespace */
      ic = getc(stream);
      nrchars++;
      while (isspace(ic))
      {
        ic = getc(stream);
        nrchars++;
      }
      if (ic != EOF)
        ungetc(ic, stream);
      nrchars--;
    }
    if (!*format)
      break; /* end of format */

    if (*format != '%')
    {
      ic = getc(stream);
      nrchars++;
      if (ic != *format++)
        break; /* error */
      continue;
    }
    format++;
    if (*format == '%')
    {
      ic = getc(stream);
      nrchars++;
      if (ic == '%')
      {
        format++;
        continue;
      }
      else
        break;
    }
    flags = 0;
    if (*format == '*')
    {
      format++;
      flags |= FL_NOASSIGN;
    }
    if (isdigit(*format))
    {
      flags |= FL_WIDTHSPEC;
      for (width = 0; isdigit(*format);)
        width = width * 10 + *format++ - '0';
    }

    switch (*format)
    {
    case 'h':
      flags |= FL_SHORT;
      format++;
      break;
    case 'l':
      flags |= FL_LONG;
      format++;
      break;
    case 'L':
      flags |= FL_LONGDOUBLE;
      format++;
      break;
    }
    kind = *format;
    if ((kind != 'c') && (kind != '[') && (kind != 'n'))
    {
      do
      {
        ic = getc(stream);
        nrchars++;
      } while (isspace(ic));
      if (ic == EOF)
        break; /* outer while */
    }
    else if (kind != 'n')
    { /* %c or %[ */
      ic = getc(stream);
      if (ic == EOF)
        break; /* outer while */
      nrchars++;
    }
    switch (kind)
    {
    default:
      /* not recognized, like %q */
      return conv || (ic != EOF) ? done : EOF;
      break;
    case 'n':
      if (!(flags & FL_NOASSIGN))
      { /* silly, though */
        if (flags & FL_SHORT)
          *va_arg(ap, short *) = (short)nrchars;
        else if (flags & FL_LONG)
          *va_arg(ap, long *) = (long)nrchars;
        else
          *va_arg(ap, int *) = (int)nrchars;
      }
      break;
    case 'p': /* pointer */
      set_pointer(flags);
      /* fallthrough */
    case 'b': /* binary */
    case 'd': /* decimal */
    case 'i': /* general integer */
    case 'o': /* octal */
    case 'u': /* unsigned */
    case 'x': /* hexadecimal */
    case 'X': /* ditto */
      if (!(flags & FL_WIDTHSPEC) || width > NUMLEN)
        width = NUMLEN;
      if (!width)
        return done;

      str = o_collect(ic, stream, kind, width, &base);
      if (str < inp_buf || (str == inp_buf && (*str == '-' || *str == '+')))
        return done;

      /*
       * Although the length of the number is str-inp_buf+1
       * we don't add the 1 since we counted it already
       */
      nrchars += str - inp_buf;

      if (!(flags & FL_NOASSIGN))
      {
        if (kind == 'd' || kind == 'i')
          val = strtol(inp_buf, &tmp_string, base);
        else
          val = strtoul(inp_buf, &tmp_string, base);
        if (flags & FL_LONG)
          *va_arg(ap, unsigned long *) = (unsigned long)val;
        else if (flags & FL_SHORT)
          *va_arg(ap, unsigned short *) = (unsigned short)val;
        else
          *va_arg(ap, unsigned *) = (unsigned)val;
      }
      break;
    case 'c':
      if (!(flags & FL_WIDTHSPEC))
        width = 1;
      if (!(flags & FL_NOASSIGN))
        str = va_arg(ap, char *);
      if (!width)
        return done;

      while (width && ic != EOF)
      {
        if (!(flags & FL_NOASSIGN))
          *str++ = (char)ic;
        if (--width)
        {
          ic = getc(stream);
          nrchars++;
        }
      }

      if (width)
      {
        if (ic != EOF)
          ungetc(ic, stream);
        nrchars--;
      }
      break;
    case 's':
      if (!(flags & FL_WIDTHSPEC))
        width = 0xffff;
      if (!(flags & FL_NOASSIGN))
        str = va_arg(ap, char *);
      if (!width)
        return done;

      while (width && ic != EOF && !isspace(ic))
      {
        if (!(flags & FL_NOASSIGN))
          *str++ = (char)ic;
        if (--width)
        {
          ic = getc(stream);
          nrchars++;
        }
      }
      /* terminate the string */
      if (!(flags & FL_NOASSIGN))
        *str = '\0';
      if (width)
      {
        if (ic != EOF)
          ungetc(ic, stream);
        nrchars--;
      }
      break;
    case '[':
      if (!(flags & FL_WIDTHSPEC))
        width = 0xffff;
      if (!width)
        return done;

      if (*++format == '^')
      {
        reverse = 1;
        format++;
      }
      else
        reverse = 0;

      for (str = Xtable; str < &Xtable[NR_CHARS]; str++)
        *str = 0;

      if (*format == ']')
        Xtable[*format++] = 1;

      while (*format && *format != ']')
      {
        Xtable[*format++] = 1;
        if (*format == '-')
        {
          format++;
          if (*format && *format != ']' && *(format) >= *(format - 2))
          {
            int c;

            for (c = *(format - 2) + 1; c <= *format; c++)
              Xtable[c] = 1;
            format++;
          }
          else
            Xtable['-'] = 1;
        }
      }
      if (!*format)
        return done;

      if (!(Xtable[ic] ^ reverse))
      {
        /* MAT 8/9/96 no match must return character */
        ungetc(ic, stream);
        return done;
      }

      if (!(flags & FL_NOASSIGN))
        str = va_arg(ap, char *);

      do
      {
        if (!(flags & FL_NOASSIGN))
          *str++ = (char)ic;
        if (--width)
        {
          ic = getc(stream);
          nrchars++;
        }
      } while (width && ic != EOF && (Xtable[ic] ^ reverse));

      if (width)
      {
        if (ic != EOF)
          ungetc(ic, stream);
        nrchars--;
      }
      if (!(flags & FL_NOASSIGN))
      { /* terminate string */
        *str = '\0';
      }
      break;
#ifndef NOFLOAT
    case 'e':
    case 'E':
    case 'f':
    case 'g':
    case 'G':
      if (!(flags & FL_WIDTHSPEC) || width > NUMLEN)
        width = NUMLEN;

      if (!width)
        return done;
      str = f_collect(ic, stream, width);

      if (str < inp_buf || (str == inp_buf && (*str == '-' || *str == '+')))
        return done;

      /*
       * Although the length of the number is str-inp_buf+1
       * we don't add the 1 since we counted it already
       */
      nrchars += str - inp_buf;

      if (!(flags & FL_NOASSIGN))
      {
        ld_val = strtod(inp_buf, &tmp_string);
        if (flags & FL_LONGDOUBLE)
          *va_arg(ap, long double *) = (long double)ld_val;
        else if (flags & FL_LONG)
          *va_arg(ap, double *) = (double)ld_val;
        else
          *va_arg(ap, float *) = (float)ld_val;
      }
      break;
#endif
    } /* end switch */
    conv++;
    if (!(flags & FL_NOASSIGN) && kind != 'n')
      done++;
    format++;
  }
  return conv || (ic != EOF) ? done : EOF;
}
