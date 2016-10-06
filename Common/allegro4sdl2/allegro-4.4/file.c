/*         ______   ___    ___ 
 *        /\  _  \ /\_ \  /\_ \ 
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___ 
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      File I/O.
 *
 *      By Shawn Hargreaves.
 *
 *      _pack_fdopen() and related modifications by Annie Testes.
 *
 *      Evert Glebbeek added the support for relative filenames:
 *      make_absolute_filename(), make_relative_filename() and
 *      is_relative_filename().
 *
 *      Peter Wang added support for packfile vtables.
 *
 *      See readme.txt for copyright information.
 *
 * ****** CJ CHANGES IN THIS FILE: Renamed pack_fopen to __old_pack_fopen
 */


#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "allegro.h"
#include "allegro/internal/aintern.h"

#ifndef ALLEGRO_MPW
   #include <sys/stat.h>
#endif

#ifdef ALLEGRO_UNIX
   #include <pwd.h>                 /* for tilde expansion */
#endif

#ifdef ALLEGRO_WINDOWS
   #include "winalleg.h" /* for GetTempPath */
#endif

#ifndef O_BINARY
   #define O_BINARY  0
#endif

/* permissions to use when opening files */
#ifndef ALLEGRO_MPW

/* some OSes have no concept of "group" and "other" */
#ifndef S_IRGRP
   #define S_IRGRP   0
   #define S_IWGRP   0
#endif
#ifndef S_IROTH
   #define S_IROTH   0
   #define S_IWOTH   0
#endif

#define OPEN_PERMS   (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

#endif /* !ALLEGRO_MPW */


static PACKFILE_VTABLE normal_vtable;


/* create_packfile:
 *  Helper function for creating a PACKFILE structure.
 */
static PACKFILE *create_packfile(int is_normal_packfile)
{
   PACKFILE *f;

    f = malloc(sizeof(PACKFILE));
    
   if (f == NULL) {
      *allegro_errno = ENOMEM;
      return NULL;
   }

  f->vtable = &normal_vtable;
  f->userdata = f;
  f->is_normal_packfile = TRUE;

  f->normal.buf_pos = f->normal.buf;
  f->normal.flags = 0;
  f->normal.buf_size = 0;
  f->normal.filename = NULL;
  f->normal.passdata = NULL;
  f->normal.passpos = NULL;
  f->normal.parent = NULL;
  f->normal.pack_data = NULL;
  f->normal.unpack_data = NULL;
  f->normal.todo = 0;

   return f;
}



/* free_packfile:
 *  Helper function for freeing the PACKFILE struct.
 */
static void free_packfile(PACKFILE *f)
{
   if (f) {

      free(f);
   }
}

/* _pack_fdopen:
 *  Converts the given file descriptor into a PACKFILE. The mode can have
 *  the same values as for pack_fopen() and must be compatible with the
 *  mode of the file descriptor. Unlike the libc fdopen(), pack_fdopen()
 *  is unable to convert an already partially read or written file (i.e.
 *  the file offset must be 0).
 *  On success, it returns a pointer to a file structure, and on error it
 *  returns NULL and stores an error code in errno. An attempt to read
 *  a normal file in packed mode will cause errno to be set to EDOM.
 */
PACKFILE *_pack_fdopen(int fd, AL_CONST char *mode)
{
   PACKFILE *f, *f2;
   long header = FALSE;
   int c;

   if ((f = create_packfile(TRUE)) == NULL)
      return NULL;

   ASSERT(f->is_normal_packfile);

   while ((c = *(mode++)) != 0) {
      switch (c) {
         case 'r': case 'R': f->normal.flags &= ~PACKFILE_FLAG_WRITE; break;
      }
   }

         /* read a 'real' file */
         f->normal.todo = lseek(fd, 0, SEEK_END);  /* size of the file */
         if (f->normal.todo < 0) {
            *allegro_errno = errno;
            free_packfile(f);
            return NULL;
         }

         lseek(fd, 0, SEEK_SET);

         f->normal.hndl = fd;

   return f;
}

/* pack_fopen:
 *  Opens a file according to mode, which may contain any of the flags:
 *  'r': open file for reading.
 *  'w': open file for writing, overwriting any existing data.
 *  'p': open file in 'packed' mode. Data will be compressed as it is
 *       written to the file, and automatically uncompressed during read
 *       operations. Files created in this mode will produce garbage if
 *       they are read without this flag being set.
 *  '!': open file for writing in normal, unpacked mode, but add the value
 *       F_NOPACK_MAGIC to the start of the file, so that it can be opened
 *       in packed mode and Allegro will automatically detect that the
 *       data does not need to be decompressed.
 *
 *  Instead of these flags, one of the constants F_READ, F_WRITE,
 *  F_READ_PACKED, F_WRITE_PACKED or F_WRITE_NOPACK may be used as the second 
 *  argument to fopen().
 *
 *  On success, fopen() returns a pointer to a file structure, and on error
 *  it returns NULL and stores an error code in errno. An attempt to read a 
 *  normal file in packed mode will cause errno to be set to EDOM.
 */
PACKFILE *__old_pack_fopen(AL_CONST char *filename, AL_CONST char *mode)
{
   char tmp[1024];
   int fd;
   ASSERT(filename);

   if (strpbrk(mode, "wW"))  /* write mode? */
       return NULL;
   else
      fd = _al_open(filename, O_RDONLY | O_BINARY, OPEN_PERMS);

   if (fd < 0) {
      *allegro_errno = errno;
      return NULL;
   }

    printf("opened %s\n", filename);
    
   return _pack_fdopen(fd, mode);
}

/* pack_fclose:
 *  Closes a file after it has been read or written.
 *  Returns zero on success. On error it returns an error code which is
 *  also stored in errno. This function can fail only when writing to
 *  files: if the file was opened in read mode it will always succeed.
 */
int pack_fclose(PACKFILE *f)
{
   int ret;

   if (!f)
      return 0;

   ASSERT(f->vtable);
   ASSERT(f->vtable->pf_fclose);

   ret = f->vtable->pf_fclose(f->userdata);
   if (ret != 0)
      *allegro_errno = errno;

   free_packfile(f);

   return ret;
}

/* pack_fseek:
 *  Like the stdio fseek() function, but only supports forward seeks 
 *  relative to the current file position.
 */
int pack_fseek(PACKFILE *f, int offset)
{
   ASSERT(f);
   ASSERT(offset >= 0);

   return f->vtable->pf_fseek(f->userdata, offset);
}

/* pack_getc:
 *  Returns the next character from the stream f, or EOF if the end of the
 *  file has been reached.
 */
int pack_getc(PACKFILE *f)
{
   ASSERT(f);
   ASSERT(f->vtable);
   ASSERT(f->vtable->pf_getc);

   return f->vtable->pf_getc(f->userdata);
}

/* pack_feof:
 *  pack_feof() returns nonzero as soon as you reach the end of the file. It 
 *  does not wait for you to attempt to read beyond the end of the file,
 *  contrary to the ISO C feof() function.
 */
int pack_feof(PACKFILE *f)
{
   ASSERT(f);
   ASSERT(f->vtable);
   ASSERT(f->vtable->pf_feof);

   return f->vtable->pf_feof(f->userdata);
}

/* pack_mgetw:
 *  Reads a 16 bit int from a file, using motorola byte-ordering.
 */
int pack_mgetw(PACKFILE *f)
{
   int b1, b2;
   ASSERT(f);

   if ((b1 = pack_getc(f)) != EOF)
      if ((b2 = pack_getc(f)) != EOF)
         return ((b1 << 8) | b2);

   return EOF;
}

/* pack_mgetl:
 *  Reads a 32 bit long from a file, using motorola byte-ordering.
 */
long pack_mgetl(PACKFILE *f)
{
   int b1, b2, b3, b4;
   ASSERT(f);

   if ((b1 = pack_getc(f)) != EOF)
      if ((b2 = pack_getc(f)) != EOF)
         if ((b3 = pack_getc(f)) != EOF)
            if ((b4 = pack_getc(f)) != EOF)
               return (((long)b1 << 24) | ((long)b2 << 16) |
                       ((long)b3 << 8) | (long)b4);

   return EOF;
}

/* pack_fread:
 *  Reads n bytes from f and stores them at memory location p. Returns the 
 *  number of items read, which will be less than n if EOF is reached or an 
 *  error occurs. Error codes are stored in errno.
 */
long pack_fread(void *p, long n, PACKFILE *f)
{
   ASSERT(f);
   ASSERT(f->vtable);
   ASSERT(f->vtable->pf_fread);
   ASSERT(p);
   ASSERT(n >= 0);

   return f->vtable->pf_fread(p, n, f->userdata);
}




/***************************************************
 ************ "Normal" packfile vtable *************
 ***************************************************

   Ideally this would be the only section which knows about the details
   of "normal" packfiles. However, this ideal is still being violated in
   many places (partly due to the API, and partly because it would be
   quite a lot of work to move the _al_normal_packfile_details field
   into the userdata field of the PACKFILE structure.
*/

static int normal_fclose(void *_f);
static int normal_getc(void *_f);
static long normal_fread(void *p, long n, void *_f);
static int normal_fseek(void *_f, int offset);
static int normal_feof(void *_f);
static int normal_ferror(void *_f);
static int normal_refill_buffer(PACKFILE *f);


static PACKFILE_VTABLE normal_vtable =
{
   normal_fclose,
   normal_getc,
   0,
   normal_fread,
   0,
   0,
   normal_fseek,
   normal_feof,
   normal_ferror
};

static int normal_fclose(void *_f)
{
   PACKFILE *f = _f;
   int ret;


   if (f->normal.parent) {
      ret = pack_fclose(f->normal.parent);
   }
   else {
      ret = close(f->normal.hndl);
      if (ret != 0)
         *allegro_errno = errno;
   }

   if (f->normal.passdata) {
      free(f->normal.passdata);
      f->normal.passdata = NULL;
      f->normal.passpos = NULL;
   }

   return ret;
}

/* normal_no_more_input:
 *  Return non-zero if the number of bytes remaining in the file (todo) is
 *  less than or equal to zero.
 *
 *  However, there is a special case.  If we are reading from a LZSS
 *  compressed file, the latest call to lzss_read() may have suspended while
 *  writing out a sequence of bytes due to the output buffer being too small.
 *  In that case the `todo' count would be decremented (possibly to zero),
 *  but the output isn't yet completely written out.
 */
static INLINE int normal_no_more_input(PACKFILE *f)
{
   /* see normal_refill_buffer() to see when lzss_read() is called */
   return (f->normal.todo <= 0);
}

static int normal_getc(void *_f)
{
   PACKFILE *f = _f;

   f->normal.buf_size--;
   if (f->normal.buf_size > 0)
      return *(f->normal.buf_pos++);

   if (f->normal.buf_size == 0) {
      if (normal_no_more_input(f))
         f->normal.flags |= PACKFILE_FLAG_EOF;
      return *(f->normal.buf_pos++);
   }

   return normal_refill_buffer(f);
}

static long normal_fread(void *p, long n, void *_f)
{
   PACKFILE *f = _f;
   unsigned char *cp = (unsigned char *)p;
   long i;
   int c;

   for (i=0; i<n; i++) {
      if ((c = normal_getc(f)) == EOF)
         break;

      *(cp++) = c;
   }

   return i;
}

static int normal_fseek(void *_f, int offset)
{
   PACKFILE *f = _f;
   int i;

   if (f->normal.flags & PACKFILE_FLAG_WRITE)
      return -1;

   *allegro_errno = 0;

   /* skip forward through the buffer */
   if (f->normal.buf_size > 0) {
      i = MIN(offset, f->normal.buf_size);
      f->normal.buf_size -= i;
      f->normal.buf_pos += i;
      offset -= i;
      if ((f->normal.buf_size <= 0) && normal_no_more_input(f))
         f->normal.flags |= PACKFILE_FLAG_EOF;
   }

   /* need to seek some more? */
   if (offset > 0) {
      i = MIN(offset, f->normal.todo);

        {
         if (f->normal.parent) {
            /* pass the seek request on to the parent file */
            pack_fseek(f->normal.parent, i);
         }
         else {
            /* do a real seek */
            lseek(f->normal.hndl, i, SEEK_CUR);
         }
         f->normal.todo -= i;
         if (normal_no_more_input(f))
            f->normal.flags |= PACKFILE_FLAG_EOF;
      }
   }

   if (*allegro_errno)
      return -1;
   else
      return 0;
}

static int normal_feof(void *_f)
{
   PACKFILE *f = _f;

   return (f->normal.flags & PACKFILE_FLAG_EOF);
}

static int normal_ferror(void *_f)
{
   PACKFILE *f = _f;

   return (f->normal.flags & PACKFILE_FLAG_ERROR);
}

/* normal_refill_buffer:
 *  Refills the read buffer. The file must have been opened in read mode,
 *  and the buffer must be empty.
 */
static int normal_refill_buffer(PACKFILE *f)
{
   int i, sz, done, offset;

   if (f->normal.flags & PACKFILE_FLAG_EOF)
      return EOF;

   if (normal_no_more_input(f)) {
      f->normal.flags |= PACKFILE_FLAG_EOF;
      return EOF;
   }

   if (f->normal.parent) {
        {
         f->normal.buf_size = pack_fread(f->normal.buf, MIN(F_BUF_SIZE, f->normal.todo), f->normal.parent);
      } 
      if (f->normal.parent->normal.flags & PACKFILE_FLAG_EOF)
         f->normal.todo = 0;
      if (f->normal.parent->normal.flags & PACKFILE_FLAG_ERROR)
         goto Error;
   }
   else {
      f->normal.buf_size = MIN(F_BUF_SIZE, f->normal.todo);

      offset = lseek(f->normal.hndl, 0, SEEK_CUR);
      done = 0;

      errno = 0;
      sz = read(f->normal.hndl, f->normal.buf, f->normal.buf_size);

      while (sz+done < f->normal.buf_size) {
         if ((sz < 0) && ((errno != EINTR) && (errno != EAGAIN)))
            goto Error;

         if (sz > 0)
            done += sz;

         lseek(f->normal.hndl, offset+done, SEEK_SET);
         errno = 0;
         sz = read(f->normal.hndl, f->normal.buf+done, f->normal.buf_size-done);
      }

   }

   f->normal.todo -= f->normal.buf_size;
   f->normal.buf_pos = f->normal.buf;
   f->normal.buf_size--;
   if (f->normal.buf_size <= 0)
      if (normal_no_more_input(f))
         f->normal.flags |= PACKFILE_FLAG_EOF;

   if (f->normal.buf_size < 0)
      return EOF;
   else
      return *(f->normal.buf_pos++);

 Error:
   *allegro_errno = EFAULT;
   f->normal.flags |= PACKFILE_FLAG_ERROR;
   return EOF;
}
