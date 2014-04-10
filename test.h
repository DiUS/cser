typedef struct {
  uint32_t a;
  char *b;
  int16_t *mc[3];
  char md[16];
  unsigned char *us _Pragma("cser zeroterm");
  bool *empty;
  const char *omitted _Pragma("cser omit");
} foo;

