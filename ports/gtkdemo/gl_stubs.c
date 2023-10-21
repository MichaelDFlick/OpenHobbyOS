typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef double GLdouble;
typedef void GLvoid;
typedef long GLsizeiptr;
typedef long GLintptr;
typedef unsigned char GLubyte;
typedef char GLchar;
typedef unsigned long long GLuint64;
typedef long long GLint64;
typedef long long GLsync;
typedef unsigned int GLbitfield;
typedef int GLintptrARB;
typedef unsigned int GLhandleARB;

void glActiveTexture(GLenum t) {}
void glAttachShader(GLuint p, GLuint s) {}
void glBindAttribLocation(GLuint p, GLuint i, const GLchar *n) {}
void glBindBuffer(GLenum t, GLuint b) {}
void glBindBufferRange(GLenum t, GLuint i, GLuint b, GLintptr o, GLsizeiptr s) {}
void glBindFramebuffer(GLenum t, GLuint f) {}
void glBindRenderbuffer(GLenum t, GLuint r) {}
void glBindSampler(GLuint u, GLuint s) {}
void glBindTexture(GLenum t, GLuint tex) {}
void glBindVertexArray(GLuint a) {}
void glBlendFunc(GLenum s, GLenum d) {}
void glBlitFramebuffer(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0, GLint dx1, GLint dy1, GLbitfield m, GLenum f) {}
void glBufferData(GLenum t, GLsizeiptr s, const void *d, GLenum u) {}
void glBufferStorage(GLenum t, GLsizeiptr s, const void *d, GLbitfield f) {}
void glBufferSubData(GLenum t, GLintptr o, GLsizeiptr s, const void *d) {}
GLenum glCheckFramebufferStatus(GLenum t) { return 0x8CD5; }
void glClear(GLbitfield m) {}
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {}
GLenum glClientWaitSync(GLsync s, GLbitfield f, GLuint64 t) { return 0x911D; }
void glCompileShader(GLuint s) {}
GLuint glCreateProgram(void) { return 0; }
GLuint glCreateShader(GLenum t) { return 0; }
void glDebugMessageCallback(void *c, void *u) {}
void glDeleteBuffers(GLsizei n, const GLuint *b) {}
void glDeleteFramebuffers(GLsizei n, const GLuint *f) {}
void glDeleteMemoryObjectsEXT(GLsizei n, const GLuint *m) {}
void glDeleteProgram(GLuint p) {}
void glDeleteRenderbuffers(GLsizei n, const GLuint *r) {}
void glDeleteSamplers(GLsizei n, const GLuint *s) {}
void glDeleteSemaphoresEXT(GLsizei n, const GLuint *s) {}
void glDeleteShader(GLuint s) {}
void glDeleteSync(GLsync s) {}
void glDeleteTextures(GLsizei n, const GLuint *t) {}
void glDeleteVertexArrays(GLsizei n, const GLuint *a) {}
void glDetachShader(GLuint p, GLuint s) {}
void glDisable(GLenum c) {}
void glDrawArraysInstanced(GLenum m, GLint f, GLsizei c, GLsizei i) {}
void glDrawArraysInstancedBaseInstance(GLenum m, GLint f, GLsizei c, GLsizei i, GLuint b) {}
void glEnable(GLenum c) {}
void glEnableVertexAttribArray(GLuint i) {}
GLsync glFenceSync(GLenum c, GLbitfield f) { return 0; }
void glFramebufferRenderbuffer(GLenum t, GLenum a, GLenum rt, GLuint rb) {}
void glFramebufferTexture2D(GLenum t, GLenum a, GLenum ta, GLuint tex, GLint l) {}
void glGenBuffers(GLsizei n, GLuint *b) {}
void glGenerateMipmap(GLenum t) {}
void glGenFramebuffers(GLsizei n, GLuint *f) {}
void glGenRenderbuffers(GLsizei n, GLuint *r) {}
void glGenSamplers(GLsizei n, GLuint *s) {}
void glGenTextures(GLsizei n, GLuint *t) {}
void glGenVertexArrays(GLsizei n, GLuint *a) {}
void glGetFramebufferParameteriv(GLenum t, GLenum p, GLint *v) {}
void glGetIntegerv(GLenum p, GLint *v) {}
void glGetProgramInfoLog(GLuint p, GLsizei b, GLsizei *l, GLchar *i) {}
void glGetProgramiv(GLuint p, GLenum pname, GLint *v) {}
void glGetShaderInfoLog(GLuint s, GLsizei b, GLsizei *l, GLchar *i) {}
void glGetShaderiv(GLuint s, GLenum pname, GLint *v) {}
void glGetShaderSource(GLuint s, GLsizei b, GLsizei *l, GLchar *src) {}
const GLubyte* glGetString(GLenum n) { return (const GLubyte*)"stub"; }
const GLubyte* glGetStringi(GLenum n, GLuint i) { return (const GLubyte*)"stub"; }
void glGetTexImage(GLenum t, GLint l, GLenum f, GLenum ty, void *p) {}
void glGetTexLevelParameteriv(GLenum t, GLint l, GLenum p, GLint *v) { if(v) *v = 0; }
GLint glGetUniformLocation(GLuint p, const GLchar *n) { return -1; }
void glLinkProgram(GLuint p) {}
void* glMapBufferRange(GLenum t, GLintptr o, GLsizeiptr s, GLbitfield a) { return 0; }
void glObjectLabel(GLenum ns, GLuint n, GLsizei l, const GLchar *n2) {}
void glPixelStorei(GLenum p, GLint a) {}
void glPopDebugGroupKHR(void) {}
void glPushDebugGroupKHR(GLenum ns, GLuint id, GLsizei l, const GLchar *m) {}
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void *p) {}
void glRenderbufferStorage(GLenum t, GLenum f, GLsizei w, GLsizei h) {}
void glSamplerParameteri(GLuint s, GLenum p, GLint v) {}
void glScissor(GLint x, GLint y, GLsizei w, GLsizei h) {}
void glShaderSource(GLuint s, GLsizei c, const GLchar * const *str, const GLint *l) {}
void glTexImage2D(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void *p) {}
void glTexParameteri(GLenum t, GLenum p, GLint v) {}
void glTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void *p) {}
void glUniform1i(GLint l, GLint v) {}
GLboolean glUnmapBuffer(GLenum t) { return 1; }
void glUseProgram(GLuint p) {}
void glVertexAttribDivisor(GLuint i, GLuint d) {}
void glVertexAttribIPointer(GLuint i, GLint s, GLenum t, GLsizei st, const void *p) {}
void glVertexAttribPointer(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void *p) {}
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {}
void glWaitSemaphoreEXT(GLuint s, GLuint n, const GLuint *b, const GLuint64 *o) {}
void glWaitSync(GLsync s, GLbitfield f, GLint64 t) {}

int epoxy_gl_version(void) { return 0; }
int epoxy_has_gl_extension(const char *e) { return 0; }

void* cairo_script_interpreter_create(void) { return 0; }
void cairo_script_interpreter_destroy(void *c) {}
int cairo_script_interpreter_feed_string(void *c, const char *s, int l) { return 0; }
void cairo_script_interpreter_install_hooks(void *c, void *h) {}

int gdk_load_jpeg(const char *f, void **d, int *w, int *h, int *st) { return 0; }
int gdk_load_tiff(const char *f, void **d, int *w, int *h, int *st) { return 0; }
int gdk_load_png(const char *f, void **d, int *w, int *h, int *st) { return 0; }
int gdk_save_jpeg(const char *f, void *d, int w, int h, int st) { return 0; }
int gdk_save_tiff(const char *f, void *d, int w, int h, int st) { return 0; }
int gdk_save_png(const char *f, void *d, int w, int h, int st) { return 0; }

int FT_Get_PS_Font_Info(void *face, void *info) { (void)face; (void)info; return -1; }
int FT_Get_BDF_Property(void *face, const char *name, void *property) { (void)face; (void)name; (void)property; return -1; }
