import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useChatStore } from '../chat'

describe('Chat Store', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
  })

  it('should initialize with empty sessions', () => {
    const store = useChatStore()
    expect(store.currentSessionId).toBeNull()
    expect(store.sessions).toEqual({})
  })

  it('should create a session', () => {
    const store = useChatStore()
    store.createSession({
      id: 'session-1',
      name: 'Test Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })

    expect(store.sessions['session-1']).toBeDefined()
    expect(store.currentSessionId).toBe('session-1')
  })

  it('should not override current session if already set', () => {
    const store = useChatStore()
    store.createSession({
      id: 'session-1',
      name: 'First Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })

    store.createSession({
      id: 'session-2',
      name: 'Second Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })

    expect(store.currentSessionId).toBe('session-1')
  })

  it('should add a message', () => {
    const store = useChatStore()
    store.createSession({
      id: 'session-1',
      name: 'Test Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })

    store.addMessage({
      id: 'msg-1',
      sessionId: 'session-1',
      senderType: 'user',
      senderId: 'user-1',
      senderName: 'Test User',
      content: 'Hello',
      status: 'pending',
      createdAt: Date.now(),
    })

    expect(store.messages['session-1']).toHaveLength(1)
    expect(store.messages['session-1'][0].content).toBe('Hello')
  })

  it('should update message status', () => {
    const store = useChatStore()
    store.createSession({
      id: 'session-1',
      name: 'Test Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })

    store.addMessage({
      id: 'msg-1',
      sessionId: 'session-1',
      senderType: 'user',
      senderId: 'user-1',
      senderName: 'Test User',
      content: 'Hello',
      status: 'pending',
      createdAt: Date.now(),
    })

    store.updateMessageStatus('msg-1', 'completed')
    expect(store.messages['session-1'][0].status).toBe('completed')
  })

  it('should return empty array for current messages when no session', () => {
    const store = useChatStore()
    expect(store.currentMessages).toEqual([])
  })

  it('should set current session', () => {
    const store = useChatStore()
    store.createSession({
      id: 'session-1',
      name: 'Test Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })

    store.createSession({
      id: 'session-2',
      name: 'Test Session 2',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })

    store.setCurrentSession('session-2')
    expect(store.currentSessionId).toBe('session-2')
  })

  it('should manage pending messages', () => {
    const store = useChatStore()

    store.addPendingMessage('msg-1')
    store.addPendingMessage('msg-2')

    expect(store.pendingMessages).toContain('msg-1')
    expect(store.pendingMessages).toContain('msg-2')

    store.removePendingMessage('msg-1')

    expect(store.pendingMessages).not.toContain('msg-1')
    expect(store.pendingMessages).toContain('msg-2')
  })

  it('should update session updatedAt on message add', () => {
    const store = useChatStore()
    const initialTime = Date.now() - 1000

    store.createSession({
      id: 'session-1',
      name: 'Test Session',
      createdAt: initialTime,
      updatedAt: initialTime,
      messages: [],
    })

    store.addMessage({
      id: 'msg-1',
      sessionId: 'session-1',
      senderType: 'user',
      senderId: 'user-1',
      senderName: 'Test User',
      content: 'Hello',
      status: 'pending',
      createdAt: Date.now(),
    })

    expect(store.sessions['session-1'].updatedAt).toBeGreaterThan(initialTime)
  })

  it('should create messages array if not exists', () => {
    const store = useChatStore()
    // 创建 session 但不带 messages 数组
    store.sessions['session-1'] = {
      id: 'session-1',
      name: 'Test Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
    }

    store.addMessage({
      id: 'msg-1',
      sessionId: 'session-1',
      senderType: 'user',
      senderId: 'user-1',
      senderName: 'Test User',
      content: 'Hello',
      status: 'pending',
      createdAt: Date.now(),
    })

    expect(store.messages['session-1']).toHaveLength(1)
  })
})
