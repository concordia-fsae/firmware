use std::collections::VecDeque;

pub trait DataPathEvent: Clone + Copy {
    type Channel: Eq + Clone + Copy;

    fn channel(&self) -> Self::Channel;
}

#[derive(Debug)]
pub struct DataPath<Event: DataPathEvent> {
    channel: Event::Channel,
    pending: VecDeque<Event>,
    latest: Option<Event>,
}

impl<Event: DataPathEvent> DataPath<Event> {
    pub fn new(channel: Event::Channel) -> Self {
        Self {
            channel,
            pending: VecDeque::new(),
            latest: None,
        }
    }

    pub fn channel(&self) -> Event::Channel {
        self.channel
    }

    pub fn clear(&mut self) {
        self.pending.clear();
        self.latest = None;
    }

    pub fn push(&mut self, event: Event) -> bool {
        if event.channel() != self.channel {
            return false;
        }
        self.latest = Some(event);
        self.pending.push_back(event);
        true
    }

    pub fn pop(&mut self) -> Option<Event> {
        self.pending.pop_front()
    }

    pub fn count(&self) -> u32 {
        self.pending.len() as u32
    }

    pub fn latest(&self) -> Option<Event> {
        self.latest
    }
}
