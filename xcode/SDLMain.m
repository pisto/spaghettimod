/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
       Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
       Non-NIB-Code & other changes: Max Horn <max@quendi.de>

    Feel free to customize this file to suit your needs
*/

#import "SDL.h"
#import "SDLMain.h"
#import <sys/param.h> /* for MAXPATHLEN */
#import <unistd.h>






#define kSAUERBRATEN @"sauerbraten"

@interface NSString(Extras)
@end
@implementation NSString(Extras)
- (NSString*)expand 
{
    NSMutableString *str = [NSMutableString string];
    [str setString:self];
    [str replaceOccurrencesOfString:@":s" withString:kSAUERBRATEN options:0 range:NSMakeRange(0, [str length])]; 
    return str;
}
@end







/* For some reaon, Apple removed setAppleMenu from the headers in 10.4,
 but the method still is there and works. To avoid warnings, we declare
 it ourselves here. */
@interface NSApplication(SDL_Missing_Methods)
- (void)setAppleMenu:(NSMenu *)menu;
@end

/* Use this flag to determine whether we use SDLMain.nib or not */
#define		SDL_USE_NIB_FILE	0

static int    gArgc;
static char  **gArgv;
static BOOL   gFinderLaunch;
static BOOL   gCalledAppMainline = FALSE;

static NSString *getApplicationName(void)
{
    NSDictionary *dict;
    NSString *appName = 0;

    /* Determine the application name */
    dict = (NSDictionary *)CFBundleGetInfoDictionary(CFBundleGetMainBundle());
    if (dict)
        appName = [dict objectForKey: @"CFBundleName"];
    
    if (![appName length])
        appName = [[NSProcessInfo processInfo] processName];

    return appName;
}


@interface SDLApplication : NSApplication
@end

@implementation SDLApplication
/* Invoked from the Quit menu item */
- (void)terminate:(id)sender
{
    /* Post a SDL_QUIT event */
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

// ADDED SO IT DOESN'T BEEP BECAUSE WE ENABLE SDL_ENABLEAPPEVENTS
- (void)sendEvent:(NSEvent *)anEvent
{
	if( NSKeyDown == [anEvent type] || NSKeyUp == [anEvent type] ) {
		if( [anEvent modifierFlags] & NSCommandKeyMask ) 
			[super sendEvent: anEvent];
	} else 
		[super sendEvent: anEvent];
}
@end

/* The main class of the application, the application's delegate */
@implementation SDLMain


static void setApplicationMenu(void)
{
    /* warning: this code is very odd */
    NSMenu *appleMenu;
    NSMenuItem *menuItem;
    NSString *title;
    NSString *appName;
    
    appName = getApplicationName();
    appleMenu = [[NSMenu alloc] initWithTitle:@""];
    
    /* Add menu items */
    title = [@"About " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    title = [@"Hide " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

    menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    [menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

    [appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    title = [@"Quit " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

    
    /* Put menu into the menubar */
    menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:appleMenu];
    [[NSApp mainMenu] addItem:menuItem];

    /* Tell the application object that this is now the application menu */
    [NSApp setAppleMenu:appleMenu];

    /* Finally give up our references to the objects */
    [appleMenu release];
    [menuItem release];
}

/* Create a window menu */
static void setupWindowMenu(void)
{
    NSMenu      *windowMenu;
    NSMenuItem  *windowMenuItem;
    NSMenuItem  *menuItem;

    windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    
    /* "Minimize" item */
    menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItem:menuItem];
    [menuItem release];
    
    /* Put menu into the menubar */
    windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
    [windowMenuItem setSubmenu:windowMenu];
    [[NSApp mainMenu] addItem:windowMenuItem];
    
    /* Tell the application object that this is now the window menu */
    [NSApp setWindowsMenu:windowMenu];

    /* Finally give up our references to the objects */
    [windowMenu release];
    [windowMenuItem release];
}

/* Replacement for NSApplicationMain */
static void CustomApplicationMain (int argc, char **argv)
{
    NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];
    SDLMain				*sdlMain;

    /* Ensure the application object is initialised */
    [SDLApplication sharedApplication];
    
    BOOL server = NO;
    int i;
    for(i = 0; i < argc; i++) if(strcmp(argv[i], "-d") == 0) { server = YES; break; }
    if(!server) /* stop being an agent program (LSUIElement=1 in plist) and bring forward */
    {   
        ProcessSerialNumber psn;
        GetCurrentProcess(&psn);
        OSStatus err = TransformProcessType(&psn, kProcessTransformToForegroundApplication);
        if(SetFrontProcess(&psn) == 0) [SDLApplication sharedApplication];
    }

    /* Set up the menubar */
    [NSApp setMainMenu:[[NSMenu alloc] init]];
    setApplicationMenu();
    setupWindowMenu();

    /* Create SDLMain and make it the app delegate */
    sdlMain = [[SDLMain alloc] init];
    [NSApp setDelegate:sdlMain];
    
    /* Start the main event loop */
    [NSApp run];
    
    [sdlMain release];
    [pool release];
}



/*
 * Catch document open requests...this lets us notice files when the app
 *  was launched by double-clicking a document, or when a document was
 *  dragged/dropped on the app's icon. You need to have a
 *  CFBundleDocumentsType section in your Info.plist to get this message,
 *  apparently.
 *
 * Files are added to gArgv, so to the app, they'll look like command line
 *  arguments. Previously, apps launched from the finder had nothing but
 *  an argv[0].
 *
 * This message may be received multiple times to open several docs on launch.
 *
 * This message is ignored once the app's mainline has been called.
 */
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
    const char *temparg;
    size_t arglen;
    char *arg;
    char **newargv;

    if (!gFinderLaunch)  /* MacOS is passing command line args. */
        return FALSE;

    if (gCalledAppMainline)  /* app has started, ignore this document. */
        return FALSE;

    temparg = [filename UTF8String];
    arglen = SDL_strlen(temparg) + 1;
    arg = (char *) SDL_malloc(arglen);
    if (arg == NULL)
        return FALSE;

    newargv = (char **) realloc(gArgv, sizeof (char *) * (gArgc + 2));
    if (newargv == NULL)
    {
        SDL_free(arg);
        return FALSE;
    }
    gArgv = newargv;

    SDL_strlcpy(arg, temparg, arglen);
    gArgv[gArgc++] = arg;
    gArgv[gArgc] = NULL;
    return TRUE;
}

- (id)init 
{
    if((self = [super init])) 
    {
        NSString *path = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"Contents/gamedata"];
        NSFileManager *fm = [NSFileManager defaultManager];
        if ([fm fileExistsAtPath:path]) 
        {
            _dataPath = [path retain];
        } 
        else  // development setup
        {
            path = [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
            // search up the folder till find a folder containing packages, or a game application containing packages
            while ([path length] > 1) 
            {
                path = [path stringByDeletingLastPathComponent];
                NSString *probe = [[path stringByAppendingPathComponent:[@":s.app" expand]] stringByAppendingPathComponent:@"Contents/gamedata"];
                if ([fm fileExistsAtPath:[probe stringByAppendingPathComponent:@"packages"]]) 
                {
                    NSLog(@"game download folder structure detected - consider using svn if you really want to develop...");
                    _dataPath = [probe retain];
                    break;
                } 
                else if ([fm fileExistsAtPath:[path stringByAppendingPathComponent:@"packages"]]) 
                {
                    NSLog(@"svn folder structure detected");
                    _dataPath = [path retain];
                    break;
                }        
            }
        }
        // userpath: directory where user files are kept - typically /Users/<name>/Application Support/sauerbraten
        FSRef folder;
        path = nil;
        if (FSFindFolder(kUserDomain, kApplicationSupportFolderType, NO, &folder) == noErr) 
        {
            CFURLRef url = CFURLCreateFromFSRef(kCFAllocatorDefault, &folder);
            path = [(NSURL *)url path];
            CFRelease(url);
            path = [path stringByAppendingPathComponent:kSAUERBRATEN];
            NSFileManager *fm = [NSFileManager defaultManager];
            if (![fm fileExistsAtPath:path]) [fm createDirectoryAtPath:path attributes:nil]; // ensure it exists    
        }
        _userPath = [path retain];
    }
    return self;
}

/* Called when the internal event loop has just started running */
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
    /* Hand off to main application code */
    gCalledAppMainline = TRUE;
    
    NSString *resolution = @"800 x 600";
    BOOL windowed = YES;
    
    NSArray *res = [resolution componentsSeparatedByString:@" x "];
    NSMutableArray *args = [NSMutableArray array];
    if(windowed) [args addObject:@"-t"];
    [args addObject:[NSString stringWithFormat:@"-w%@", [res objectAtIndex:0]]];
    [args addObject:[NSString stringWithFormat:@"-h%@", [res objectAtIndex:1]]];
    [args addObject:@"-z32"]; // otherwise seems to have a fondness to use -z16
    [args addObject:[NSString stringWithFormat:@"-q%@", _userPath]];
    
    // set up path, argv, env for C
    const int argc = [args count]+1;
    const char **argv = (const char**)malloc(sizeof(char*)*(argc + 2)); // {path, <args>, NULL};
    argv[0] = [[[NSBundle mainBundle] executablePath] fileSystemRepresentation];        
    argv[argc] = NULL;
    int i;
    for(i = 1; i < argc; i++) argv[i] = [[args objectAtIndex:i-1] UTF8String];

    assert(chdir([_dataPath fileSystemRepresentation]) == 0);
     
    setenv("SDL_SINGLEDISPLAY", "1", 1);
    setenv("SDL_ENABLEAPPEVENTS", "1", 1); // makes Command-H, Command-M and Command-Q work at least when not in fullscreen
    
    SDL_main(argc, (char**)argv);
    [NSApp terminate];
}

@end


#ifdef main
#  undef main
#endif


/* Main entry point to executable - should *not* be SDL_main! */
int main (int argc, char **argv)
{
    /* Copy the arguments into a global variable */
    /* This is passed if we are launched by double-clicking */
    if ( argc >= 2 && strncmp (argv[1], "-psn", 4) == 0 ) {
        gArgv = (char **) SDL_malloc(sizeof (char *) * 2);
        gArgv[0] = argv[0];
        gArgv[1] = NULL;
        gArgc = 1;
        gFinderLaunch = YES;
    } else {
        int i;
        gArgc = argc;
        gArgv = (char **) SDL_malloc(sizeof (char *) * (argc+1));
        for (i = 0; i <= argc; i++)
            gArgv[i] = argv[i];
        gFinderLaunch = NO;
    }

#if SDL_USE_NIB_FILE
    [SDLApplication poseAsClass:[NSApplication class]];
    NSApplicationMain (argc, argv);
#else
    CustomApplicationMain (argc, argv);
#endif
    return 0;
}