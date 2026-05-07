#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include"userprog/process.h"

static void syscall_handler (struct intr_frame *);
static struct lock filesys_lock;


void
syscall_init (void)
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

//sdx
void check_valid_ptr (const void *vaddr)
{

  //second check is to ensure the pointer is in the memory user space
  //third check is to ensure the virtual address maps to a valid physical address
  if (vaddr == NULL || !is_user_vaddr (vaddr) || pagedir_get_page (thread_current ()->pagedir, vaddr) == NULL)
  {
    //STILL NOT DONE -- WAITING FOR EXIT() TO BE IMPLEMENTED
    thread_current()->exit_status = -1;
    thread_exit ();
  }
}

static int
fd_add (struct file *f)
{
  struct thread *t = thread_current ();
  for (int i = 2; i < 128; i++)        
    if (t->fd_table[i] == NULL)
      {
        t->fd_table[i] = f;
        return i;
      }
  return -1;                             
}

static struct file *
fd_get (int fd)
{
  if (fd < 2 || fd >= 128)
    return NULL;
  return thread_current ()->fd_table[fd];
}

static void
fd_close (int fd)
{
  if (fd < 2 || fd >= 128)
    return;
  struct thread *t = thread_current ();
  if (t->fd_table[fd] != NULL)
    {
      file_close (t->fd_table[fd]);
      t->fd_table[fd] = NULL;
    }
}

static bool
sys_create (const char *file, unsigned initial_size)
{
  lock_acquire (&filesys_lock);
  bool ok = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
  return ok;
}

static bool
sys_remove (const char *file)
{
  lock_acquire (&filesys_lock);
  bool ok = filesys_remove (file);
  lock_release (&filesys_lock);
  return ok;
}

static int
sys_open (const char *file)
{
  lock_acquire (&filesys_lock);
  struct file *f = filesys_open (file);
  lock_release (&filesys_lock);
  if (f == NULL)
    return -1;
  int fd = fd_add (f);
  if (fd == -1)
    file_close (f);
  return fd;
}

static int
sys_filesize (int fd)
{
  struct file *f = fd_get (fd);
  if (f == NULL) return -1;
  lock_acquire (&filesys_lock);
  int size = file_length (f);
  lock_release (&filesys_lock);
  return size;
}

static int
sys_read (int fd, void *buffer, unsigned size)
{
  if (fd == 0)                          
    {
      for (unsigned i = 0; i < size; i++)
        ((uint8_t *) buffer)[i] = input_getc ();
      return size;
    }
  struct file *f = fd_get (fd);
  if (f == NULL) return -1;
  lock_acquire (&filesys_lock);
  int bytes = file_read (f, buffer, size);
  lock_release (&filesys_lock);
  return bytes;
}

static int
sys_write (int fd, const void *buffer, unsigned size)
{
  if (fd == 1)                        
    {
      putbuf (buffer, size);
      return size;
    }
  struct file *f = fd_get (fd);
  if (f == NULL) return -1;
  lock_acquire (&filesys_lock);
  int bytes = file_write (f, buffer, size);
  lock_release (&filesys_lock);
  return bytes;
}

static void
sys_seek (int fd, unsigned position)
{
  struct file *f = fd_get (fd);
  if (f == NULL) return;
  lock_acquire (&filesys_lock);
  file_seek (f, position);
  lock_release (&filesys_lock);
}

static unsigned
sys_tell (int fd)
{
  struct file *f = fd_get (fd);
  if (f == NULL) return -1;
  lock_acquire (&filesys_lock);
  unsigned pos = file_tell (f);
  lock_release (&filesys_lock);
  return pos;
}

static void
sys_close (int fd)
{
  lock_acquire (&filesys_lock);
  fd_close (fd);
  lock_release (&filesys_lock);
}


static void
sys_exit(int status)
{
  thread_current()->exit_status=status;
  thread_exit();
}

static tid_t
sys_exec(const char* cmd_line)
{
  tid_t pid=process_execute(cmd_line);
  return (pid==TID_ERROR)? -1 : pid;
}

static int
sys_wait(tid_t pid)
{
    return process_wait(pid);
}


//sdx
static void
syscall_handler (struct intr_frame *f)
{
  //Validate the stack pointer itself
  check_valid_ptr (f->esp);

  //Stack pointer like an array of integers
  int *args = (int *) f->esp;

  //Read system call number
  int syscall_number = args[0];

  switch (syscall_number)
  {
    case SYS_HALT:
    shutdown_power_off();
      break;

    case SYS_EXIT:
      // 1 argument: int status
      check_valid_ptr(&args[1]);
       sys_exit(args[1]);
      break;

    case SYS_EXEC:
    {
      // 1 argument: const char *cmd_line
      check_valid_ptr(&args[1]);
      const char *cmd_line = (const char *) args[1];
      check_valid_ptr(cmd_line); // Validate string pointer
      f->eax = sys_exec(cmd_line);
      break;
    }

    case SYS_WAIT:
      // 1 argument: pid_t pid
      check_valid_ptr(&args[1]);
       f->eax = sys_wait(args[1]);
      break;

    case SYS_CREATE:
    {
      // 2 arguments: const char *file, unsigned initial_size
      check_valid_ptr(&args[1]);
      check_valid_ptr(&args[2]);
      const char *file = (const char *) args[1];
      unsigned initial_size = (unsigned) args[2];
      check_valid_ptr(file); // Validate the string pointer
       f->eax = sys_create(file, initial_size);
      break;
    }

    case SYS_REMOVE:
    {
      // 1 argument: const char *file
      check_valid_ptr(&args[1]);
      const char *file = (const char *) args[1];
      check_valid_ptr(file); // Validate the string pointer
       f->eax = sys_remove(file);
      break;
    }

    case SYS_OPEN:
    {
      // 1 argument: const char *file
      check_valid_ptr(&args[1]);
      const char *file = (const char *) args[1];
      check_valid_ptr(file); // Validate the string pointer
       f->eax = sys_open(file);
      break;
    }

    case SYS_FILESIZE:
      // 1 argument: int fd
      check_valid_ptr(&args[1]);
       f->eax = sys_filesize(args[1]);
      break;

    case SYS_READ:
    {
      // 3 arguments: int fd, void *buffer, unsigned size
      check_valid_ptr(&args[1]);
      check_valid_ptr(&args[2]);
      check_valid_ptr(&args[3]);

      int fd = args[1];
      void *buffer = (void *) args[2];
      unsigned size = (unsigned) args[3];
      check_valid_ptr(buffer); // Validate the buffer pointer

       f->eax = sys_read(fd, buffer, size);
      break;
    }

    case SYS_WRITE:
    {
      // 3 arguments: int fd, const void *buffer, unsigned size
      check_valid_ptr(&args[1]);
      check_valid_ptr(&args[2]);
      check_valid_ptr(&args[3]);

      int fd = args[1];
      const void *buffer = (const void *) args[2];
      unsigned size = (unsigned) args[3];
      check_valid_ptr(buffer); // Validate the buffer pointer
       f->eax = sys_write(fd, buffer, size);
      break;
    }

    case SYS_SEEK:
      // 2 arguments: int fd, unsigned position
      check_valid_ptr(&args[1]);
      check_valid_ptr(&args[2]);
       sys_seek(args[1], args[2]);
      break;

    case SYS_TELL:
      // 1 argument: int fd
      check_valid_ptr(&args[1]);
       f->eax = sys_tell(args[1]);
      break;

    case SYS_CLOSE:
      // 1 argument: int fd
      check_valid_ptr(&args[1]);
       sys_close(args[1]);
      break;

    default:
      // If the user asks for a system call that doesn't exist -> kill
      thread_current()->exit_status = -1;
      thread_exit();
  }
}