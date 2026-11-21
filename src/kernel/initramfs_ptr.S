[bits 64]

section .initptr

global initramfs_ptr
global initramfs_size
global initramfs_lba

initramfs_ptr:  dq 0
initramfs_size: dq 0
initramfs_lba:  dq 0
