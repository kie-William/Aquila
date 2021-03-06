#include <core/system.h>
#include <fs/vfs.h>

int vfs_mknod(const char *path, mode_t mode, dev_t dev, struct uio *uio, struct inode **ref)
{
    int ret = 0;
    struct vfs_path *p = NULL;
    char **tokens = NULL;

    /* if path is NULL pointer, or path is empty string, return NULL */
    if (!path ||  !*path)
        return -ENOENT;

    char *_path = NULL;
    if ((ret = vfs_parse_path(path, uio, &_path)))
        goto error;

    /* Canonicalize Path */
    tokens = canonicalize_path(_path);

    /* Get mountpoint & path */
    p = vfs_get_mountpoint(tokens);

    struct vnode cur, next;

    cur.super  = p->mountpoint;
    cur.ino    = p->mountpoint->ino;
    cur.mode   = S_IFDIR; /* XXX */
    next.super = p->mountpoint;

    char *name = NULL;
    char **tok = p->tokens;

    while (tok) {
        char *token = *tok;

        if (!*(tok + 1)) {
            name = token;
            break;
        }

        if ((ret = vfs_vfind(&cur, token, &next)))
            goto error;

        memcpy(&cur, &next, sizeof(cur));
        ++tok;
    }

    if ((ret = vfs_vmknod(&cur, name, mode, dev, uio, ref)))
        goto error;

    free_tokens(tokens);
    kfree(p);
    kfree(_path);
    return 0;

error:
    if (tokens)
        free_tokens(tokens);
    if (p)
        kfree(p);
    if (_path)
        kfree(_path);
    return ret;
}

int vfs_mkdir(const char *path, mode_t mode, struct uio *uio, struct inode **ref)
{
    return vfs_mknod(path, S_IFDIR | mode, 0, uio, ref);
}

int vfs_creat(const char *path, mode_t mode, struct uio *uio, struct inode **ref)
{
    return vfs_mknod(path, S_IFREG | mode, 0, uio, ref);
}
