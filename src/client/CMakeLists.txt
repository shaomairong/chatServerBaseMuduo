# 定义了一个SRC_LIST变量，包含了该目录下所有的源文件
aux_source_directory(. SRC_LIST)

# 指定生成可执行文件
add_executable(ChatClient ${SRC_LIST})
# 指定可执行文件链接时需要依赖的库文件
target_link_libraries(ChatClient pthread)
#客户端时有读写二个线程，读线程专门接受chatServer发过来的消息；写线程接受用户的输入然后发送业务数据;
#因为时基于控制台的客户端
