#include <driver/vga.h>
#include <zjunix/log.h>
#include <zjunix/slub.h>
#include <zjunix/fs/fat.h>
#include "fat.h"
#include "utils.h"

u8 mk_dir_buf[32];
FILE file_create;

//删除文件
u32 fs_rm(u8 *filename) {
    u32 clus;
    u32 next_clus;
    FILE mk_dir;

    if (fs_open(&mk_dir, filename) == 1)
        goto fs_rm_err;

    //已删除的标志位
    mk_dir.entry.data[0] = 0xE5;

    //将分配给他的所有数据块都清空
    clus = get_start_cluster(&mk_dir);

    while (clus != 0 && clus <= fat_info.total_data_clusters + 1) {
        if (get_fat_entry_value(clus, &next_clus) == 1)
            goto fs_rm_err;

        if (fs_modify_fat(clus, 0) == 1)
            goto fs_rm_err;

        clus = next_clus;
    }

    if (fs_close(&mk_dir) == 1)
        goto fs_rm_err;

    return 0;
fs_rm_err:
    return 1;
}

//移动文件
u32 fs_mv(u8 *src, u8 *dest) {
    u32 i;
    FILE mk_dir;
    u8 filename11[13];

    //如果源文件不存在，那么返回错误
    if (fs_open(&mk_dir, src) == 1)
        goto fs_mv_err;

    //创建目标文件
    if (fs_create_with_attr(dest, mk_dir.entry.data[11]) == 1)
        goto fs_mv_err;

    //复制文件目录项
    for (i = 0; i < 32; i++)
        mk_dir_buf[i] = mk_dir.entry.data[i];

    //新路径赋值
    for (i = 0; i < 11; i++)
        mk_dir_buf[i] = filename11[i];

    if (fs_open(&file_create, dest) == 1)
        goto fs_mv_err;

    //将源文件目录项复制到目标文件
    for (i = 0; i < 32; i++)
        file_create.entry.data[i] = mk_dir_buf[i];

    if (fs_close(&file_create) == 1)
        goto fs_mv_err;

    //将源文件标志位已删除
    mk_dir.entry.data[0] = 0xE5;

    if (fs_close(&mk_dir) == 1)
        goto fs_mv_err;

    return 0;
fs_mv_err:
    return 1;
}

u32 fs_mkdir(u8 *filename) {
    u32 i;
    FILE mk_dir;
    FILE file_creat;

    if (fs_create_with_attr(filename, 0x30) == 1)
        goto fs_mkdir_err;

    if (fs_open(&mk_dir, filename) == 1)
        goto fs_mkdir_err;

    mk_dir_buf[0] = '.';
    for (i = 1; i < 11; i++)
        mk_dir_buf[i] = 0x20;

    mk_dir_buf[11] = 0x30;
    for (i = 12; i < 32; i++)
        mk_dir_buf[i] = 0;

    if (fs_write(&mk_dir, mk_dir_buf, 32) == 1)
        goto fs_mkdir_err;

    fs_lseek(&mk_dir, 0);

    mk_dir_buf[20] = mk_dir.entry.data[20];
    mk_dir_buf[21] = mk_dir.entry.data[21];
    mk_dir_buf[26] = mk_dir.entry.data[26];
    mk_dir_buf[27] = mk_dir.entry.data[27];

    if (fs_write(&mk_dir, mk_dir_buf, 32) == 1)
        goto fs_mkdir_err;

    mk_dir_buf[0] = '.';
    mk_dir_buf[1] = '.';

    for (i = 2; i < 11; i++)
        mk_dir_buf[i] = 0x20;

    mk_dir_buf[11] = 0x30;
    for (i = 12; i < 32; i++)
        mk_dir_buf[i] = 0;

    set_u16(mk_dir_buf + 20, (file_creat.dir_entry_pos >> 16) & 0xFFFF);
    set_u16(mk_dir_buf + 26, file_creat.dir_entry_pos & 0xFFFF);

    if (fs_write(&mk_dir, mk_dir_buf, 32) == 1)
        goto fs_mkdir_err;

    for (i = 28; i < 32; i++)
        mk_dir.entry.data[i] = 0;

    if (fs_close(&mk_dir) == 1)
        goto fs_mkdir_err;

    return 0;
fs_mkdir_err:
    return 1;
}

//读取文件
u32 fs_cat(u8 *path) {
    u8 filename[12];
    FILE cat_file;

    //打开文件
    if (0 != fs_open(&cat_file, path)) {
        log(LOG_FAIL, "File %s open failed", path);
        return 1;
    }

    //读取文件内容
    u32 file_size = get_entry_filesize(cat_file.entry.data);
    //给文件要读取的内容非配一定大小的空间
    u8 *buf = (u8 *)kmalloc(file_size + 1);
    fs_read(&cat_file, buf, file_size);
    buf[file_size] = 0;
    kernel_printf("%s\n", buf);
    fs_close(&cat_file);
    //kfree(buf);
    return 0;
}

//str之后加上param
void append_dir(char *nowdir,char *newdir,char *param)
{
    int i=0;
        
    for(i=0;i<64;i++)
        newdir[i]=0;
    i=0;
    while(nowdir[i]!=0){
        newdir[i]=nowdir[i];
        ++i;
    }
    int j=0;
    while(param[j]){
        newdir[i]=param[j];
        ++i;
        ++j;
    }
}

//创建文件
u32 fs_touch(u8 *filename)
{
    if(fs_create(filename) == 1)
        goto fs_touch_error;
        return 0;
fs_touch_error:
        return 1;
}

u32 fs_makedir(u8 *filename)
{
    if(fs_mkdir(filename) == 1)
        goto fs_touch_error;
        return 0;
fs_touch_error:
        return 1;
}

//删除文件
u32 fs_remove(u8 *filename)
{
    if(fs_rm(filename) == 1)
        return 1;
    else
        return 0;
}
    
//改变文件路径
u32 fs_changedir(u8 *newdir,u8 *nowdir,u8 *param)
{
    append_dir(nowdir,newdir,param);
    kernel_memcpy(nowdir,newdir,64*sizeof(char));
    return 0;
}

//返回到上一层目录
u32 fs_prev_dir(u8 *nowdir)
{
    u8 *p;
    p=&nowdir[59];
    while(*p==0)
    {
        p--;   
    }
    while(*p!='/')
    {
        *p=0;
        p--;
    }
    *p=0;
    return 0;
}