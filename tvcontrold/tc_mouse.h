/**
 *  Functions to operate with an X server
 */
#ifndef TC_MOUSE_H_INCLUDED
#define TC_MOUSE_H_INCLUDED

/**
 *  Initialize the mouse operations.
 */
int tc_mouse_init(void);

/**
 *  Move the mouse cursor.
 */
void tc_mouse_move(void);

/**
 *  Release the resources of the mouse.
 */
void tc_mouse_release(void);

#endif /* TC_MOUSE_H_INCLUDED */
