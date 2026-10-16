// Runs unprivileged after system installation. Never deletes plugins as root.
#import <AppKit/AppKit.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#ifndef OPENPREAMP_INSTALL_VERSION
#define OPENPREAMP_INSTALL_VERSION "1.0.2"
#endif
static NSString* releaseVersion = @OPENPREAMP_INSTALL_VERSION;
static NSArray<NSString*>* relativePaths() {
    return @[@"Library/Audio/Plug-Ins/VST3/OpenPreamp.vst3",
             @"Library/Audio/Plug-Ins/Components/OpenPreamp.component",
             @"Library/Audio/Plug-Ins/CLAP/OpenPreamp.clap",
             @"Library/Audio/Plug-Ins/LV2/OpenPreamp.lv2"];
}
static BOOL safePath(NSString* home, NSString* relative, struct stat* status) {
    // Do not traverse redirected plugin folders or remove a symlink target.
    if (![relativePaths() containsObject:relative]) return NO;
    NSString* path = home;
    for (NSString* part in [relative pathComponents]) {
        path = [path stringByAppendingPathComponent:part];
        if (lstat(path.fileSystemRepresentation, status) != 0 || !S_ISDIR(status->st_mode) || S_ISLNK(status->st_mode)) return NO;
    }
    return YES;
}
static NSString* pluginVersion(NSString* path) {
    if ([path.pathExtension isEqualToString:@"lv2"]) {
        NSString* ttl = [NSString stringWithContentsOfFile:[path stringByAppendingPathComponent:@"dsp.ttl"] encoding:NSUTF8StringEncoding error:nil];
        if (![ttl containsString:@"https://github.com/MistaMin/OpenPreamp"]) return nil;
        NSRegularExpression* regex = [NSRegularExpression regularExpressionWithPattern:@"doap:revision\\s+\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" options:0 error:nil];
        NSTextCheckingResult* match = [regex firstMatchInString:ttl options:0 range:NSMakeRange(0,ttl.length)];
        return match ? [ttl substringWithRange:[match rangeAtIndex:1]] : @"unknown";
    }
    NSDictionary* info = [NSDictionary dictionaryWithContentsOfFile:[path stringByAppendingPathComponent:@"Contents/Info.plist"]];
    NSString* identity = info[@"CFBundleIdentifier"];
    if (![identity.lowercaseString isEqualToString:@"com.opengrid.openpreamp"]) return nil;
    id version = info[@"CFBundleShortVersionString"] ?: info[@"CFBundleVersion"];
    return [version isKindOfClass:NSString.class] ? version : @"unknown";
}
static NSArray<NSDictionary*>* scan(NSString* home) {
    NSMutableArray* result = [NSMutableArray array];
    for (NSString* relative in relativePaths()) {
        struct stat status;
        if (!safePath(home,relative,&status)) continue;
        NSString* version = pluginVersion([home stringByAppendingPathComponent:relative]);
        if (!version || (![version isEqualToString:@"unknown"] && [version compare:releaseVersion options:NSNumericSearch] == NSOrderedDescending)) continue;
        [result addObject:@{@"relative":relative,@"version":version,@"device":@(status.st_dev),@"inode":@(status.st_ino),@"mtime":@(status.st_mtimespec.tv_sec),@"nsec":@(status.st_mtimespec.tv_nsec)}];
    }
    return result;
}
typedef BOOL (^TrashOperation)(NSURL*, NSError**);
static NSArray<NSString*>* cleanup(NSString* home, NSArray<NSDictionary*>* approved, BOOL consent, TrashOperation trash) {
    NSMutableArray* failures = [NSMutableArray array];
    if (!consent) return failures;
    for (NSDictionary* item in approved) {
        NSString* relative = item[@"relative"];
        struct stat status;
        // Recheck after consent: a replaced bundle or changed version is left alone.
        if (!safePath(home,relative,&status) || status.st_dev != [item[@"device"] unsignedLongLongValue] || status.st_ino != [item[@"inode"] unsignedLongLongValue] || status.st_mtimespec.tv_sec != [item[@"mtime"] longLongValue] || status.st_mtimespec.tv_nsec != [item[@"nsec"] longLongValue]) {
            [failures addObject:relative]; continue;
        }
        NSString* path = [home stringByAppendingPathComponent:relative];
        if (![pluginVersion(path) isEqualToString:item[@"version"]]) { [failures addObject:relative]; continue; }
        NSError* error = nil;
        if (!trash([NSURL fileURLWithPath:path],&error)) [failures addObject:[NSString stringWithFormat:@"%@: %@",relative,error.localizedDescription ?: @"could not move to Trash"]];
    }
    return failures;
}
static void fixture(NSString* home, NSUInteger index, NSString* version, NSString* identity) {
    NSString* path = [home stringByAppendingPathComponent:relativePaths()[index]];
    NSFileManager* fm = NSFileManager.defaultManager;
    [fm createDirectoryAtPath:[path stringByAppendingPathComponent:@"Contents"] withIntermediateDirectories:YES attributes:nil error:nil];
    [@{@"CFBundleIdentifier":identity,@"CFBundleShortVersionString":version} writeToFile:[path stringByAppendingPathComponent:@"Contents/Info.plist"] atomically:YES];
}
static int selfTest() {
    NSFileManager* fm = NSFileManager.defaultManager;
    NSString* home = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    [fm createDirectoryAtPath:home withIntermediateDirectories:YES attributes:nil error:nil];
    __block int checks=0, failures=0;
    void (^check)(BOOL,NSString*) = ^(BOOL ok,NSString* name){ ++checks; if(!ok)++failures; printf("%s %s\n",ok?"PASS":"FAIL",name.UTF8String); };
    check(scan(home).count==0,@"clean home needs no prompt");
    fixture(home,0,@"1.0.0",@"com.OpenGrid.OpenPreamp");
    fixture(home,1,releaseVersion,@"com.OpenGrid.OpenPreamp");
    fixture(home,2,@"9.0.0",@"com.opengrid.openpreamp");
    auto candidates=scan(home);check(candidates.count==2,@"old/current copies detected; newer copy preserved");
    __block int moves=0;
    TrashOperation fakeTrash=^BOOL(NSURL* url,NSError** error){++moves;return [fm moveItemAtPath:url.path toPath:[home stringByAppendingPathComponent:[@"trashed-" stringByAppendingString:url.lastPathComponent]] error:error];};
    cleanup(home,candidates,NO,fakeTrash);check(moves==0 && scan(home).count==2,@"declining consent changes nothing");
    check(cleanup(home,candidates,YES,fakeTrash).count==0 && moves==2,@"consent moves exactly the approved copies");
    check([fm fileExistsAtPath:[home stringByAppendingPathComponent:relativePaths()[2]]],@"newer copy remains after cleanup");
    fixture(home,0,@"1.0.0",@"other.plugin");check(scan(home).count==0,@"unrelated bundle identity is ignored");
    fixture(home,0,@"1.0.0",@"com.OpenGrid.OpenPreamp");candidates=scan(home);
    fixture(home,0,@"9.0.0",@"com.OpenGrid.OpenPreamp");check(cleanup(home,candidates,YES,fakeTrash).count==1 && moves==2,@"version changed after consent is preserved");
    NSString* lv2=[home stringByAppendingPathComponent:relativePaths()[3]];
    [fm createDirectoryAtPath:lv2 withIntermediateDirectories:YES attributes:nil error:nil];
    [@"<https://github.com/MistaMin/OpenPreamp> doap:revision \"1.0.0\" ;" writeToFile:[lv2 stringByAppendingPathComponent:@"dsp.ttl"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
    check(scan(home).count==1,@"LV2 identity and version detected");
    [fm removeItemAtPath:lv2 error:nil];[fm createSymbolicLinkAtPath:lv2 withDestinationPath:[home stringByAppendingPathComponent:relativePaths()[2]] error:nil];
    check(scan(home).count==0,@"bundle symlinks ignored");
    NSString* vstFolder=[home stringByAppendingPathComponent:@"Library/Audio/Plug-Ins/VST3"];
    [fm removeItemAtPath:vstFolder error:nil];[fm createSymbolicLinkAtPath:vstFolder withDestinationPath:[home stringByAppendingPathComponent:@"Library/Audio/Plug-Ins/CLAP"] error:nil];
    check(scan(home).count==0,@"redirected parent folder ignored");
    [fm removeItemAtPath:home error:nil];printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
static NSAlert* makeAlert(NSArray<NSDictionary*>* candidates) {
    NSMutableArray* labels=[NSMutableArray array];for(NSDictionary* item in candidates)[labels addObject:[NSString stringWithFormat:@"~/%@ (version %@)",item[@"relative"],item[@"version"]]];
    NSAlert* alert=[[NSAlert alloc] init];alert.messageText=@"Move existing user copies of OpenPreamp to Trash?";
    alert.informativeText=[NSString stringWithFormat:@"OpenPreamp %@ is installed in /Library/Audio/Plug-Ins for all users. These copies in your user folder may take precedence in your DAW:\n\n%@\n\nMove them to your Trash to use the system installation? You can restore them later. Newer copies and other users’ files are left alone.",releaseVersion,[labels componentsJoinedByString:@"\n"]];
    [alert addButtonWithTitle:@"Move to Trash"];
    NSButton* keep=[alert addButtonWithTitle:@"Keep User Copies"];keep.keyEquivalent=@"\033";
    return alert;
}
int main(int argc,const char** argv) { @autoreleasepool {
    if(argc==2 && strcmp(argv[1],"--self-test")==0) return selfTest();
#ifdef OPENPREAMP_INSTALLER_PREVIEW
    if(argc==2 && strcmp(argv[1],"--preview")==0) {
        [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        NSAlert* alert=makeAlert(@[@{@"relative":relativePaths()[0],@"version":@"1.0.0"},@{@"relative":relativePaths()[1],@"version":@"1.0.0"}]);
        alert.messageText=@"TEST PREVIEW — no files will be changed";
        alert.informativeText=[@"This uses simulated copies for UI verification. No installed plugins are moved.\n\n" stringByAppendingString:alert.informativeText];
        [NSApp activateIgnoringOtherApps:YES];NSInteger response=[alert runModal];
        printf("Preview response %ld; no files changed\n",(long)response);return 0;
    }
#endif
    if(argc!=1 || getuid()==0 || geteuid()!=getuid()) return 1;
    struct passwd* account=getpwuid(getuid());if(!account || !account->pw_dir) return 1;
    NSString* home=[[NSString stringWithUTF8String:account->pw_dir] stringByResolvingSymlinksInPath];
    NSArray* candidates=scan(home);if(candidates.count==0)return 0;
    [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    NSAlert* alert=makeAlert(candidates);
    [NSApp activateIgnoringOtherApps:YES];BOOL consent=[alert runModal]==NSAlertFirstButtonReturn;
    NSArray* errors=cleanup(home,candidates,consent,^BOOL(NSURL* url,NSError** error){return [NSFileManager.defaultManager trashItemAtURL:url resultingItemURL:nil error:error];});
    if(errors.count){NSAlert* warning=[[NSAlert alloc]init];warning.messageText=@"Some user copies were kept";warning.informativeText=[errors componentsJoinedByString:@"\n"];[warning runModal];}
    return 0;
}}
