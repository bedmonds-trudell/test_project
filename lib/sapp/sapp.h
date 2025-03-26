 #pragma once

 /**
  * @brief Start Sub-App (sapp)
  *        See ./efi/sapp2.c for example use.
  *
  * @param name      char * ptr to name of sapp (should be unique!)
  * @param setup     function pointer to setup()
  * @param loop      function pointer to loop()
  *
  * @retval 0 On success.
  * @retval -errno Other negative errno in case of failure.
  */
 int sapp_start(const char *name, int (*setup)(), int (*loop)());