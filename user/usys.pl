#!/usr/bin/env perl
use strict;
use warnings;


my $hdr = $ARGV[0] // "../kernel/syscall.h";  # 允许通过参数传入
open my $fh, '<', $hdr or die "failed to open $hdr: $!";

my @names;
while (my $line = <$fh>) {
    chomp $line;
    $line =~ s{//.*$}{};          # 去掉 // 注释
    $line =~ s{/\*.*?\*/}{}g;    # 去掉 /* */ 注释
    if ($line =~ /#\s*define\s+SYS_([A-Za-z0-9_]+)\b/) {
        my $macro = $1;           # 保留原大小写（例如 fork 而非 FORK）
        my $name = lc($macro);    # 生成的函数名使用小写
        push @names, { func => $name, macro => $macro };
    }
}
close $fh;

# 去重并保持首次出现的顺序
my %seen;
@names = grep { !$seen{ $_->{func} }++ } @names;

print "#include \"../kernel/syscall.h\"\n\n";
print "\t.section .text\n\n";
for my $e (@names) {
    my $name = $e->{func};
    my $macro = $e->{macro};
    print "\t.globl $name\n";
    print "$name:\n";
    print "\tli a7, SYS_${macro}\n";
    print "\tecall\n";
    print "\tret\n\n";
}
