use strict;
use warnings;

use Data::Dumper;
use Test::More;
use Test::Output;

my $CLASS = 'JavaScript::V8::XS';

sub test_console {
    my $vm = $CLASS->new();
    ok($vm, "created $CLASS object");

    my @texts = (
        q<'Hello Gonzo'>,
        q<'this is a string', 1, [], {}>,
    );
    foreach my $text (@texts) {
        my $expected = $text;
        $expected =~ s/[',]//g;
        $expected = quotemeta($expected);

        foreach my $func (qw/ log debug trace info /) {
            stdout_like sub { $vm->eval("console.$func($text)"); },
                        qr/$expected/,
                        "got correct stdout from $func for <$text>";
        }

        foreach my $func (qw/ warn error exception /) {
            stderr_like sub { $vm->eval("console.$func($text)"); },
                        qr/$expected/,
                        "got correct stderr from $func for <$text>";
        }
    }
}

sub main {
    use_ok($CLASS);

    test_console();
    done_testing;
    return 0;
}

exit main();
