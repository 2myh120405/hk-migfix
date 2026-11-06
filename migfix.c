/* Minimal bypass for backboardd's startup migration wait (iOS 16.x).
 *
 * The stock backboardd blocks the system shell (SpringBoard) until "data
 * migration" completes.  On a lab device /private/var starts empty every boot,
 * so that migration never completes and the boot progress bar stays at ~99%.
 *
 * This dylib replaces -[BKSystemShellSentinel
 * checkInAndWaitForDataMigration:completionBlock:] so that the original
 * implementation is called with a nil (NO) wait argument.  Everything else
 * (the reply handling, the sentinel state machine) stays native.
 *
 * Build:
 *   xcrun --sdk iphoneos clang -arch arm64 -target arm64-apple-ios16.0 \
 *       -dynamiclib -O2 -undefined dynamic_lookup -o migfix.dylib migfix.c
 */
#include <string.h>

typedef void *objc_id;
typedef void *objc_sel;
typedef void *objc_class;
typedef void *objc_method;
typedef void *objc_imp;

extern objc_class objc_getClass(const char *name);
extern objc_sel sel_registerName(const char *name);
extern objc_method class_getInstanceMethod(objc_class cls, objc_sel sel);
extern const char *method_getTypeEncoding(objc_method m);
extern objc_imp method_getImplementation(objc_method m);
extern objc_imp class_replaceMethod(objc_class cls, objc_sel sel, objc_imp imp, const char *types);
extern const char *getprogname(void);

static objc_imp g_original;
static objc_sel g_selector;

static void migfix_checkin(objc_id self, objc_sel cmd, objc_id wait, objc_id block)
{
	(void)wait; /* the caller's YES becomes NO */
	((void (*)(objc_id, objc_sel, objc_id, objc_id))g_original)(self, cmd, (objc_id)0, block);
}

__attribute__((constructor)) static void migfix_init(void)
{
	objc_class cls;
	objc_method method;
	objc_sel sel;

	if (strcmp(getprogname(), "backboardd") != 0) return;

	cls = objc_getClass("BKSystemShellSentinel");
	if (cls == (objc_class)0) return;

	sel = sel_registerName("checkInAndWaitForDataMigration:completionBlock:");
	method = class_getInstanceMethod(cls, sel);
	if (method == (objc_method)0) return;

	g_original = method_getImplementation(method);
	g_selector = sel;
	class_replaceMethod(cls, sel, (objc_imp)migfix_checkin, method_getTypeEncoding(method));
}
