struct lnode
{
    lnode* next;
    s32 data;
};

void add(lnode* head, s32 data)
{
    while(head->next)
        head = head->next;

    head->next = (lnode*)malloc(sizeof(lnode));
    head->next->data = data;
    head->next->next = nullptr;
}

s32 main()
{
    lnode head;
    head.data = 9;
    head.next = nullptr;
    add(&head, 1);
    add(&head, 10);
    add(&head, 3);
    add(&head, 5);
    add(&head, 6);

    lnode* it = &head;
    while (it)
    {
        printf(it->data);
        it = it->next;
    }

    return 0;
}