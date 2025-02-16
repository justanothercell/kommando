SHELL = /bin/bash

delete_image:
	-sudo docker rmi kommando-sandbox-runner

stop_container:
	-sudo docker stop kommando-sandbox-runner-instance

delete_container: stop_container
	-sudo docker rm kommando-sandbox-runner-instance

build_image:
	pushd .. && make build_release && popd

	cp ../kommando include/kommando
	cp -r ../kdolib include/kdolib

	sudo docker build -t kommando-sandbox-runner .

launch_container:
	sudo docker run --read-only --tmpfs /tempdata:rw,exec,nosuid,size=100m -d --name kommando-sandbox-runner-instance -p 7878:7878 kommando-sandbox-runner

start_container:
	sudo docker start kommando-sandbox-runner-instance

observe_container:
	sudo docker logs --follow kommando-sandbox-runner-instance

shell_container:
	sudo docker exec -it kommando-sandbox-runner-instance sh

clean_all: delete_container delete_image
