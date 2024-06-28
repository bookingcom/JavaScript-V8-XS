FROM quay.io/centos/centos:stream9

RUN sed -i s/protocol=https,http/protocol=https/g /etc/yum.repos.d/* && \
    yum install -y --allowerasing curl wget xz && \
    yum install -y libxcrypt-compat make gcc-toolset-13* git && \
    export PATH=~/perl/bin:/opt/rh/gcc-toolset-13/root/usr/bin:$PATH && \
    curl -fsSL https://raw.githubusercontent.com/skaji/relocatable-perl/main/perl-install | bash -s ~/perl && \
    cpanm -n ExtUtils::MakeMaker \
        Path::Tiny \
        File::Copy::Recursive \
        JSON::PP \
        JSON::XS \
        Scalar::Util \
        Data::Dumper \
        Test::More \
        Test::Output \
        Time::HiRes \
        Test::Exception \
        Text::Trim ExtUtils::XSpp && \
    rm -rf /root/.cpanm/work

ENV PATH=/root/perl/bin:/opt/rh/gcc-toolset-13/root/usr/bin:$PATH
