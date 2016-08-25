# xunsearch-dev docker
# created by hightman.20150826
#
# START COMMAND:

# docker run -d --name xunsearch -p 8383:8383 -p 8384:8384 \
# -v /var/xunsearch/data:/usr/local/xunsearch/data hightman/xunsearch:latest
#
FROM ubuntu:14.04
MAINTAINER hightman, hightman@twomice.net

# Install required packages
RUN apt-get update -qq
RUN apt-get install -qy --no-install-recommends \
	wget make gcc g++ bzip2 zlib1g-dev 

# Download & Install xunsearch-dev
RUN cd /root && wget -qO - http://www.xunsearch.com/download/xunsearch-full-dev.tar.bz2 | tar xj
RUN cd /root/xunsearch-full-dev && sh setup.sh --prefix=/usr/local/xunsearch


RUN echo '' >> /usr/local/xunsearch/bin/xs-ctl.sh
RUN echo 'tail -f /dev/null' >> /usr/local/xunsearch/bin/xs-ctl.sh

# Configure it
#VOLUME ["/usr/local/xunsearch/data"]
EXPOSE 8383
EXPOSE 8384

WORKDIR /usr/local/xunsearch
RUN echo "#!/bin/sh" > bin/xs-docker.sh
RUN echo "rm -f tmp/pid.*" >> bin/xs-docker.sh
RUN echo "echo -n > tmp/docker.log" >> bin/xs-docker.sh
RUN echo "bin/xs-indexd -l tmp/docker.log -k start" >> bin/xs-docker.sh
RUN echo "sleep 1" >> bin/xs-docker.sh
RUN echo "bin/xs-searchd -l tmp/docker.log -k start" >> bin/xs-docker.sh
RUN echo "sleep 1" >> bin/xs-docker.sh
RUN echo "tail -f tmp/docker.log" >> bin/xs-docker.sh

ENTRYPOINT ["sh"]
CMD ["bin/xs-docker.sh"]

