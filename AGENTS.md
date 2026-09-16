stm32h743

## Function naming

- Use the project prefix `pt_` for public functions.
- Follow ThreadX-style role-based names after the prefix, such as `pt_kernel_initialize()`,
  `pt_kernel_start()`, `pt_thread_sleep()`, and `pt_timer_interrupt()`.
- Use `pt_` for internal helper functions as well, such as `pt_thread_schedule()` and
  `pt_thread_context_switch()`; do not use `_pt_` because it is reserved by C at file scope.
- Do not introduce new `ptOS_` or `tx_` function names.

## Communication

- Respond to the user in Chinese.
