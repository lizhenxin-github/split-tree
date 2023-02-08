#pragma once

#include <cstring>
/**
 * @brief 一个间接指针结构体，指向一个额外开辟的空间，实现比较功能
 *
 */

inline int strcmp_sob(char *cs, char *ct)
{
    unsigned char c1, c2;
    if (cs == NULL && ct == NULL)
        return 0;
    else if (cs == NULL)
        return -1;
    else if (ct == NULL)
        return 1;

    while (1)
    {
        c1 = *cs++;
        c2 = *ct++;
        if (c1 != c2)
            return c1 < c2 ? -1 : 1;
        if (!c1)
            break;
    }
    return 0;
}

class indirect_ptr
{
public:
    char *ptr;
    indirect_ptr()
    {
        ptr = NULL;
    }
    indirect_ptr(char *source)
    {
        ptr = source;
    }

    bool operator<(indirect_ptr a)
    { //必须加const
        return strcmp_sob(ptr, a.ptr) < 0;
    }

    bool operator==(indirect_ptr a)
    { //必须加const
        return strcmp_sob(ptr, a.ptr) == 0;
    }

    bool operator!=(indirect_ptr a)
    { //必须加const
        return strcmp_sob(ptr, a.ptr) != 0;
    }

    bool operator>(indirect_ptr a)
    { //必须加const
        return strcmp_sob(ptr, a.ptr) > 0;
    }

    bool operator<=(indirect_ptr a)
    { //必须加const
        return strcmp_sob(ptr, a.ptr) <= 0;
    }

    bool operator>=(indirect_ptr a)
    { //必须加const
        return strcmp_sob(ptr, a.ptr) >= 0;
    }

    int operator-(indirect_ptr a)
    { //间接指针的减法就是比较两者的大小
        return strcmp_sob(ptr, a.ptr);
    }
};

inline indirect_ptr indirect_null = indirect_ptr();

inline indirect_ptr indirect_max = indirect_ptr("||||");
