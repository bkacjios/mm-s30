#ifndef RT64_EXTENDED_GBI
#define RT64_EXTENDED_GBI

/*
 * S30: RT64's extended display list commands (matrix groups for frame interpolation) do nothing here; the
 * display list pointer they are given is not advanced.
 */
#define gEXMatrixGroupDecomposed(cmd, ...) do { } while (0)
#define gEXMatrixGroupSimple(cmd, ...) do { } while (0)
#define gEXPopMatrixGroup(cmd, ...) do { } while (0)

#endif
