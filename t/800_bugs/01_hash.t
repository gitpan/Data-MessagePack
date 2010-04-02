use strict;
use Data::MessagePack;
use Test::More tests => 2;

do {
    my $x = { a => undef };
    \$x->{a}; # ok
    my $y = Data::MessagePack->unpack(  Data::MessagePack->pack($x) );
    is ${\$y->{a}}, undef;
};

do {
    my $x = [ undef ];
    \$x->[0]; # ok
    my $y = Data::MessagePack->unpack(  Data::MessagePack->pack($x) );
    is ${\$y->[0]}, undef;
};

