use strict;
use warnings;
use v5.28;

use ExtUtils::MakeMaker;
use Path::Tiny;
use JSON::XS;

my @V8_CC_OPTS;
my @V8_CC_DEFS;
my @V8_CC_WRNS;
my @V8_CC_INCD;
my @V8_LD_OPTS;
my @V8_LD_LIBN;
my @V8_LD_LIBD;

use File::Fetch;
use POSIX();
use Digest::SHA;
use Archive::Tar;
use File::Path qw(make_path);
use File::Basename qw(dirname basename);
use File::Copy::Recursive qw(rmove);

my ($sysname, $machine) = (POSIX::uname)[0, -1];
$sysname = lc($sysname);

my $NODEJS_VERSION = $ENV{NODEJS_VERSION} // '20.12.1.0';
my $RUBYGEMS = $ENV{RUBYGEMS} // "https://rubygems.org/downloads";
my $NODEJS_URL = "${RUBYGEMS}/libv8-node-${NODEJS_VERSION}-${machine}-${sysname}.gem";

my $NODEJS_INTEGRITIES = {
    "20.12.1.0" => {
        darwin => {
            arm64 => "bae2867eff6c26245cd8e4c73122adba93391b2a14210e8719613afa83e437cd",
            x86_64 => "3520d312c334f6bcd8051c8c5ff3432c3430c0fd857d5a547c68975be1c2f671",
        },
        linux => {
            aarch64 => "6604cb511723d438150377ce77b957138157358fb618e713ec438fa8f3c013cf",
            x86_64 => "8d58e74037a3cf45145579289cc099c2dbd6a02a77b5a899426a63cb2c8d64e7",
        }
    },
};

my $NODEJS_INTEGRITY = $ENV{NODEJS_INTEGRITY} // $NODEJS_INTEGRITIES->{$NODEJS_VERSION}->{$sysname}->{$machine};

my $V8_BASE_DIR = $ENV{V8_BASE_DIR} // "libv8/${NODEJS_VERSION}/${sysname}/${machine}";
my $V8_INCLUDE_DIR = $ENV{V8_INCLUDE_DIR} // join('/', $V8_BASE_DIR, 'vendor', 'v8', 'include');
my $V8_LIB_DIR = $ENV{V8_LIB_DIR} // join('/', $V8_BASE_DIR, 'vendor', 'v8', "${machine}-${sysname}", "libv8", "obj");

if ( ! -d $V8_BASE_DIR ) {
    say STDERR "downloading: $NODEJS_URL";
    use Data::Dumper;

    my $fetcher = File::Fetch->new(uri => $NODEJS_URL);

    my $file = $fetcher->fetch() or die $fetcher->error;

    if ($NODEJS_INTEGRITY) {
        my $sha = Digest::SHA->new(256);
        $sha->addfile($file);
        my $sha_hex = $sha->hexdigest;
        die "Integrity check failed: expected $NODEJS_INTEGRITY, got $sha_hex" if $sha_hex ne $NODEJS_INTEGRITY;
    }

    my $tar = Archive::Tar->new;
    $tar->read($file);
    $tar->extract_file("data.tar.gz", "libv8-${NODEJS_VERSION}-${sysname}-${machine}.tar.gz") or die "Failed to extract data.tar.gz";

    $tar = Archive::Tar->new;
    $tar->read("libv8-${NODEJS_VERSION}-${sysname}-${machine}.tar.gz");
    my @files = $tar->list_files();
    @files = grep { m!^vendor/! } @files;

    for my $file (@files) {
        my $dirname = dirname($file);
        make_path("${V8_BASE_DIR}.tmp/${dirname}");
        my $basename = basename($file);
        $tar->extract_file($file, "${V8_BASE_DIR}.tmp/${file}") or die "Failed to extract $file";
    }

    unlink("libv8-${NODEJS_VERSION}-${sysname}-${machine}.tar.gz");
    unlink($file);

    rmove("${V8_BASE_DIR}.tmp", $V8_BASE_DIR);
}

push @V8_CC_INCD, $V8_INCLUDE_DIR;
push @V8_LD_LIBD, $V8_LIB_DIR;

push @V8_CC_OPTS, qw< -O2 -std=c++17 >;
push @V8_CC_WRNS, qw< all extra no-unused-parameter >;

# this is not provided any more by v8
my $V8_DATA_DIR    = $ENV{V8_DATA_DIR}    // join('/', $V8_BASE_DIR, 'data');
push @V8_CC_DEFS, "V8_DATA_DIR='${V8_DATA_DIR}'";

if ($^O eq 'linux') {
     push @V8_CC_DEFS, qw< PERL_JAVASCRIPT_V8_XS_LINUX >;

#     # these apply only on linux, where the V8 build has to ignore standard
#     # include files and instead looks at its own internal libc++ files
#     push @V8_CC_OPTS, qw< -nostdinc++ >;
#     push @V8_CC_INCD, map { "$V8_INCLUDE_DIR/$_/include" } qw< libc++ libc++abi >;
#     push @V8_CC_DEFS, qw< _LIBCPP_ABI_UNSTABLE >;
#     push @V8_CC_DEFS, join('=', '_LIBCPP_ABI_VERSION', 'Cr');

}

if ($^O eq 'darwin') {
     push @V8_CC_DEFS, qw< PERL_JAVASCRIPT_V8_XS_DARWIN >;
}

my $CC = $ENV{CC} // 'c++';
my $LD = $ENV{LD} // 'c++';

my $CC_OPTS = join(' ',
    (@V8_CC_OPTS),
    (map { "-D$_" } @V8_CC_DEFS),
    (map { "-W$_" } @V8_CC_WRNS),
    (map { "-I$_" } @V8_CC_INCD),
);

push @V8_LD_LIBN, "v8_monolith";

my $LD_OPTS = join(' ',
    (@V8_LD_OPTS),
    (map { "-L$_" } @V8_LD_LIBD),
    (map { "-l$_" } @V8_LD_LIBN),
    (map { "-Wl,-rpath,$_" } @V8_LD_LIBD),
);

WriteMakefile(
    NAME           => 'JavaScript::V8::XS',
    VERSION_FROM   => 'lib/JavaScript/V8/XS.pm',
    ABSTRACT_FROM  => 'lib/JavaScript/V8/XS.pm',
    LICENSE        => 'mit',
    MIN_PERL_VERSION => 5.028000,
    PREREQ_PM      => {
        'JSON::PP'        => 0,
        'Text::Trim'      => 0,
        'XSLoader'        => 0,
        'ExtUtils::XSpp'  => '0.11',
    },
    TEST_REQUIRES  => {
        'Data::Dumper'    => 0,
        'JSON::PP'        => 0,
        'Path::Tiny'      => 0,
        'Scalar::Util'    => 0,
        'Test::More'      => 0,
        'Test::Output'    => 0,
        'Test::Exception' => 0,
        'Text::Trim'      => 0,
        'Time::HiRes'     => 0,
    },
    AUTHOR         => [
        'Gonzalo Diethelm (gonzus@cpan.org)',
    ],

    # compiler settings
    CC                 => "$CC $CC_OPTS",
    DEFINE             => '',
    depend             => { 'v8-perl.c' => 'JavaScript-V8-XS-Context.xsp' },

    # linker settings
    LD                 => "$LD $LD_OPTS",
    OBJECT             => '$(O_FILES)',

    # precompiler settings
    XSOPT              => '-C++ -hiertype',
    TYPEMAPS           => ['perlobject.map'],

    # repo settings
    META_MERGE         => {
        'meta-spec'    => { version => 2 },
        resources      => {
            repository => {
                type   => 'git',
                url    => 'git@github.com:gonzus/JavaScript-V8-XS',
                web    => 'https://github.com/gonzus/JavaScript-V8-XS',
            },
        },
    },
);
