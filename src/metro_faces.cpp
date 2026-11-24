#include "faces.h"
#include "metro.h"
void faceDraw(int, const tm& local, bool valid) { metroDraw(local, valid); }
void faceInvalidate() { metroInvalidate(); }
bool faceAvailable(int face) { return face == 0; }
void facePhotosChanged() {}
int photoCount() { return 0; }
