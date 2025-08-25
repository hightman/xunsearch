# xunsearch-latest docker
# created by hightman.20150826
#
# START COMMAND:

# docker run -d --name xunsearch -p 8383:8383 -p 8384:8384 \
#   -v /data/xunsearch:/usr/local/xunsearch/data hightman/xunsearch:latest
#
# -------------
# Stage: build
# -------------
FROM ubuntu:18.04 as build

# Install required packages
RUN apt-get update -qq \
    && apt-get install -qy --no-install-recommends \
      tzdata ca-certificates wget make gcc g++ bzip2 zlib1g-dev

# Download dumb-init
RUN wget -qO /usr/bin/dumb-init \
      https://github.com/Yelp/dumb-init/releases/download/v1.2.5/dumb-init_1.2.5_x86_64 \
    && chmod +x /usr/bin/dumb-init

# Download & Install xunsearch-latest
RUN wget -qO - https://www.xunsearch.com/download/xunsearch-full-latest.tar.bz2 | tar -xj \
    && cd xunsearch-full-* && sh setup.sh --prefix=/usr/local/xunsearch \
    && cd .. && rm -rf xunsearch-full-* \
    && apt-get purge -y wget make gcc g++ bzip2 \
    && apt-get autoremove -y && apt-get clean && rm -rf /var/lib/apt/lists/*

# --------------
# Stage: runtime
# --------------
FROM ubuntu:18.04 as runtime
MAINTAINER hightman, hightman@twomice.net
ENV TZ Asia/Chongqing

# Copy files
COPY --from=build /usr/bin/dumb-init /usr/bin/
COPY --from=build /usr/share/zoneinfo/ /usr/share/zoneinfo
COPY --from=build /usr/local/xunsearch /usr/local/xunsearch

RUN rm -rf /usr/local/xunsearch/include \
      /usr/local/xunsearch/sdk \
      /usr/local/xunsearch/bin/xs-php

# Configure it
VOLUME /usr/local/xunsearch/data
WORKDIR /usr/local/xunsearch
EXPOSE 8383 8384

# Entry
COPY docker-entrypoint.sh entry.sh
RUN chmod +x entry.sh
ENTRYPOINT ["/usr/bin/dumb-init", "--"]
CMD ["./entry.sh"]
