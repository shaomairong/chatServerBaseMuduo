#server文件夹下面的CMakeLists.txt就要根据源码生成可执行文件了
# 定义了一个SRC_LIST变量，包含了该目录下所有的源文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_LIST)
aux_source_directory(./model MODEL_LIST)
aux_source_directory(./redis REDIS_LIST)

# 指定生成可执行文件：由后面这些源文件来生成
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST} ${REDIS_LIST})
# 指定可执行文件链接时需要依赖的库文件:生成ChatServer可执行文件需要连接muduo_net,muduo_base,mysqlclient hiredis pthread这些库
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread)