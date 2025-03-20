# xunsearch-dev docker
# created by hightman.20150826
#
# START COMMAND:

# docker run -d --name xunsearch -p 8383:8383 -p 8384:8384 \
# -v /var/xunsearch/data:/usr/local/xunsearch/data hightman/xunsearch:latest
#
FROM ubuntu:16.04
MAINTAINER hightman, hightman@twomice.net
ENV TZ Asia/Chongqing

# Install required packages
RUN apt-get update -qq \
    && apt-get install -qy --no-install-recommends \
      tzdata ca-certificates wget make gcc g++ bzip2 zlib1g-dev

# Download & Install xunsearch-dev
RUN wget -qO - https://www.xunsearch.com/download/xunsearch-full-dev.tar.bz2 | tar -xj \
    && cd xunsearch-full-* && sh setup.sh --prefix=/usr/local/xunsearch \
    && cd .. && rm -rf xunsearch-full-* \
    && apt-get purge -y wget make gcc g++ bzip2 \
    && apt-get autoremove -y && apt-get clean && rm -rf /var/lib/apt/lists/*

# Configure it
VOLUME ["/usr/local/xunsearch/data"]
EXPOSE 8383
EXPOSE 8384

# EntryPoint
WORKDIR /usr/local/xunsearch
COPY docker-entrypoint.sh entry.sh
RUN chmod +x entry.sh
CMD ["./entry.sh"]
