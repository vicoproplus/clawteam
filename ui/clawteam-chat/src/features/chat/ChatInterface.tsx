import { useEffect, useRef } from "react";
import { Send, Bot, User } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Avatar, AvatarFallback } from "@/components/ui/avatar";
import { useChat } from "@/hooks/useChat";
import { cn } from "@/lib/utils";

export function ChatInterface() {
  const { messages, inputText, isLoading, setInputText, sendMessage, setCurrentSession } =
    useChat();
  const scrollRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  // 初始化默认会话
  useEffect(() => {
    setCurrentSession("default");
  }, [setCurrentSession]);

  // 自动滚动到底部
  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [messages]);

  const handleSend = () => {
    if (inputText.trim() && !isLoading) {
      sendMessage(inputText);
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  return (
    <div className="flex h-full w-full flex-col">
      {/* 消息列表 */}
      <ScrollArea className="flex-1 p-4" ref={scrollRef}>
        <div className="mx-auto max-w-3xl space-y-4">
          {messages.length === 0 && (
            <div className="flex h-[60vh] items-center justify-center">
              <div className="text-center text-muted-foreground">
                <Bot className="mx-auto mb-4 h-12 w-12 opacity-50" />
                <p className="text-lg font-medium">ClawTeam Chat</p>
                <p className="text-sm">多智能体协作终端</p>
              </div>
            </div>
          )}
          {messages.map((msg) => (
            <div
              key={msg.id}
              className={cn(
                "flex gap-3",
                msg.senderType === "user" && "flex-row-reverse"
              )}
            >
              <Avatar className="h-8 w-8">
                <AvatarFallback
                  className={cn(
                    msg.senderType === "user"
                      ? "bg-primary text-primary-foreground"
                      : "bg-muted"
                  )}
                >
                  {msg.senderType === "user" ? (
                    <User className="h-4 w-4" />
                  ) : (
                    <Bot className="h-4 w-4" />
                  )}
                </AvatarFallback>
              </Avatar>
              <div
                className={cn(
                  "flex-1 rounded-lg px-4 py-2",
                  msg.senderType === "user"
                    ? "bg-primary text-primary-foreground ml-2"
                    : "bg-muted mr-2"
                )}
              >
                <div className="text-sm font-medium mb-1">{msg.senderName}</div>
                <div className="text-sm whitespace-pre-wrap">{msg.content}</div>
              </div>
            </div>
          ))}
          {isLoading && (
            <div className="flex gap-3">
              <Avatar className="h-8 w-8">
                <AvatarFallback className="bg-muted">
                  <Bot className="h-4 w-4" />
                </AvatarFallback>
              </Avatar>
              <div className="flex-1 rounded-lg bg-muted px-4 py-2 mr-2">
                <div className="flex items-center gap-2">
                  <div className="h-2 w-2 animate-bounce rounded-full bg-muted-foreground [animation-delay:0ms]" />
                  <div className="h-2 w-2 animate-bounce rounded-full bg-muted-foreground [animation-delay:150ms]" />
                  <div className="h-2 w-2 animate-bounce rounded-full bg-muted-foreground [animation-delay:300ms]" />
                </div>
              </div>
            </div>
          )}
        </div>
      </ScrollArea>

      {/* 输入区域 */}
      <div className="border-t p-4">
        <div className="mx-auto flex max-w-3xl gap-2">
          <Input
            ref={inputRef}
            value={inputText}
            onChange={(e) => setInputText(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder="输入消息... (Enter 发送)"
            className="flex-1"
            disabled={isLoading}
          />
          <Button onClick={handleSend} disabled={!inputText.trim() || isLoading}>
            <Send className="h-4 w-4" />
          </Button>
        </div>
      </div>
    </div>
  );
}
