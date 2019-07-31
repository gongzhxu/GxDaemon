#!/usr/bin/perl -w

use strict;
use POSIX 'setsid';
use Data::Dumper;

my $program = "";
my $cmd = "";

if(@ARGV)
{
	$program = $ARGV[0];
}

foreach my $arg(@ARGV)
{
	$cmd="$cmd $arg ";
}

print("cmd = $cmd\n");


if($cmd eq "")
{
	print("error, cmd is empty\n");
	exit(0);
}

my $pid = fork();
if($pid < 0)
{
	print("create fork error\n");
	return -1;
}
elsif($pid > 0)
{
	exit(0);
}

#close(STDOUT);
setsid();
umask(0);

$SIG{"TERM"} = "interrupt";

LOOP:

$pid = fork();
if($pid < 0)
{
        print("create fork error\n");
        return -1;
}
elsif($pid > 0)
{
	wait();
	print("restart program $program after 10s!!!\n");
	sleep(10);
	goto LOOP;	
}


print("begin run program $program!!!\n");	
exec $cmd;	

sub interrupt
{
	(my $pid) = @_;
	print("killing the program $program!!!\n");
	if($pid > 0)
	{
		kill($pid);
	}
}
