CC = xcrun -sdk iphoneos clang
CFLAGS = -arch arm64 -arch armv7 -g -Oz -Wall -Wextra -miphoneos-version-min=7.0
CFLAGS += -Iinclude
LDFLAGS =
DSYMUTIL = dsymutil
STRIP = strip
LDID = ldid

all: jetsamctl

package: jetsamctl jetsamctl_iphoneos-arm64.deb jetsamctl_iphoneos-arm.deb

jetsamctl_iphoneos-arm64.deb: jetsamctl
	rm -rf stage-iphoneos-arm64
	mkdir -p stage-iphoneos-arm64/var/jb/usr/bin
	mkdir -p stage-iphoneos-arm64/DEBIAN
	sed -e 's/@ARCH@/iphoneos-arm64/' control > stage-iphoneos-arm64/DEBIAN/control
	chmod 644 stage-iphoneos-arm64/DEBIAN/control
	install -m755 jetsamctl stage-iphoneos-arm64/var/jb/usr/bin
	dpkg-deb -b stage-iphoneos-arm64 "$@"

jetsamctl_iphoneos-arm.deb: jetsamctl
	rm -rf stage-iphoneos-arm
	mkdir -p stage-iphoneos-arm/usr/bin
	mkdir -p stage-iphoneos-arm/DEBIAN
	sed -e 's/@ARCH@/iphoneos-arm/' control> stage-iphoneos-arm/DEBIAN/control
	chmod 644 stage-iphoneos-arm/DEBIAN/control
	install -m755 jetsamctl stage-iphoneos-arm/usr/bin
	dpkg-deb -b stage-iphoneos-arm "$@"

jetsamctl: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) main.c -Iinclude -o jetsamctl
	$(DSYMUTIL) jetsamctl
	$(STRIP) jetsamctl
	$(LDID) -Sentitlements.xml jetsamctl

clean:
	rm -rf jetsamctl* *.deb stage*
