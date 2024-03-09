CC != xcrun --find cc
CFLAGS = -g -Oz -Wall -Wextra -Iinclude
IPHONE_SYSROOT != xcrun -sdk iphoneos --show-sdk-path
TV_SYSROOT != xcrun -sdk appletvos --show-sdk-path
MAC_SYSROOT != xcrun -sdk macosx --show-sdk-path
IPHONE_CFLAGS = $(CFLAGS) -miphoneos-version-min=7.0 -arch arm64 -arch armv7 -isysroot $(IPHONE_SYSROOT)
TV_CFLAGS = $(CFLAGS) -mappletvos-version-min=9.0 -arch arm64 -isysroot $(TV_SYSROOT)
MAC_CFLAGS = $(CFLAGS) -mmacosx-version-min=10.9 -arch x86_64 -arch arm64 -isysroot $(MAC_SYSROOT)
LDFLAGS =
DSYMUTIL = dsymutil
STRIP = strip
LDID = ldid
LIPO = lipo
BINS = jetsamctl_iphone jetsamctl_mac jetsamctl_tv
DEBS = jetsamctl_iphoneos-arm64.deb jetsamctl_iphoneos-arm.deb jetsamctl_darwin-amd64.deb jetsamctl_darwin-arm64.deb jetsamctl_appletvos-arm64.deb

all: $(BINS)

package: $(BINS) $(DEBS)

jetsamctl_iphoneos-arm64.deb: jetsamctl_iphone
	rm -rf stage-iphoneos-arm64
	mkdir -p stage-iphoneos-arm64/var/jb/usr/bin
	mkdir -p stage-iphoneos-arm64/DEBIAN
	sed -e 's/@ARCH@/iphoneos-arm64/' control > stage-iphoneos-arm64/DEBIAN/control
	chmod 644 stage-iphoneos-arm64/DEBIAN/control
	$(LIPO) jetsamctl_iphone -thin arm64 -output stage-iphoneos-arm64/var/jb/usr/bin/jetsamctl
	dpkg-deb --root-owner-group -b stage-iphoneos-arm64 "$@"

jetsamctl_darwin-arm64.deb: jetsamctl_mac
	rm -rf stage-darwin-arm64
	mkdir -p stage-darwin-arm64/opt/procursus/bin
	mkdir -p stage-darwin-arm64/DEBIAN
	sed -e 's/@ARCH@/darwin-arm64/' control > stage-darwin-arm64/DEBIAN/control
	chmod 644 stage-darwin-arm64/DEBIAN/control
	$(LIPO) jetsamctl_mac -thin arm64 -output stage-darwin-arm64/opt/procursus/bin/jetsamctl
	dpkg-deb --root-owner-group -b stage-darwin-arm64 "$@"

jetsamctl_darwin-amd64.deb: jetsamctl_mac
	rm -rf stage-darwin-amd64
	mkdir -p stage-darwin-amd64/opt/procursus/bin
	mkdir -p stage-darwin-amd64/DEBIAN
	sed -e 's/@ARCH@/darwin-amd64/' control > stage-darwin-amd64/DEBIAN/control
	chmod 644 stage-darwin-amd64/DEBIAN/control
	$(LIPO) jetsamctl_mac -thin x86_64 -output stage-darwin-amd64/opt/procursus/bin/jetsamctl
	dpkg-deb --root-owner-group -b stage-darwin-amd64 "$@"

jetsamctl_iphoneos-arm.deb: jetsamctl_iphone
	rm -rf stage-iphoneos-arm
	mkdir -p stage-iphoneos-arm/usr/bin
	mkdir -p stage-iphoneos-arm/DEBIAN
	sed -e 's/@ARCH@/iphoneos-arm/' control> stage-iphoneos-arm/DEBIAN/control
	chmod 644 stage-iphoneos-arm/DEBIAN/control
	install -m755 jetsamctl_iphone stage-iphoneos-arm/usr/bin/jetsamctl
	dpkg-deb --root-owner-group -b stage-iphoneos-arm "$@"

jetsamctl_appletvos-arm64.deb: jetsamctl_tv
	rm -rf stage-appletvos-arm64
	mkdir -p stage-appletvos-arm64/usr/bin
	mkdir -p stage-appletvos-arm64/DEBIAN
	sed -e 's/@ARCH@/appletvos-arm64/' control> stage-appletvos-arm64/DEBIAN/control
	chmod 644 stage-appletvos-arm64/DEBIAN/control
	install -m755 jetsamctl_tv stage-appletvos-arm64/usr/bin/jetsamctl
	dpkg-deb --root-owner-group -b stage-appletvos-arm64 "$@"


jetsamctl_iphone: main.c
	$(CC) $(IPHONE_CFLAGS) $(LDFLAGS) $(LIBS) main.c -Iinclude -o $@
	$(DSYMUTIL) $@
	$(STRIP) $@
	$(LDID) -Sentitlements.xml $@


jetsamctl_mac: main.c
	$(CC) $(MAC_CFLAGS) $(LDFLAGS) $(LIBS) main.c -Iinclude -o $@
	$(DSYMUTIL) $@
	$(STRIP) $@
	$(LDID) -S $@

jetsamctl_tv: main.c
	$(CC) $(TV_CFLAGS) $(LDFLAGS) $(LIBS) main.c -Iinclude -o $@
	$(DSYMUTIL) $@
	$(STRIP) $@
	$(LDID) -Sentitlements.xml $@

clean:
	rm -rf jetsamctl* *.deb stage*
